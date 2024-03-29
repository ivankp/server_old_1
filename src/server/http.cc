#include "server/http.hh"

#include <vector>
#include <algorithm>

#include "server/socket.hh"
#include "local_fd.hh"
#include "whole_file.hh"
#include "file_cache.hh"
#include "zlib.hh"
#include "error.hh"
#include "debug.hh"

namespace ivanp::http {
namespace {

struct mimes_dict {
  std::string s;
  std::vector<const char*> m;

  mimes_dict(const char* filename): s(whole_file(filename)) {
    size_t nlines = 0;
    for (char c : s) if (c=='\n') ++nlines;
    if (s.back()!='\n') ++nlines;
    m.reserve(nlines);
    for (char *a=s.data(), *b; ; ) {
      a += strspn(a," \t\r\n"); // trim preceding blanks
      if (!*a) break; // end
      if (*a=='#') { // comment
        a += strcspn(a+1,"\n");
        continue;
      }
      b = a + strcspn(a," \t\r\n"); // end of key
      if (strchr("\r\n",*b)) ERROR(filename); // no value
      *b = '\0';
      const char* v1 = ++b;
      b += strspn(b," \t"); // trim blanks before value
      if (strchr("\r\n",*b)) ERROR(filename); // no value
      const char* v2 = b;
      b += strcspn(b,"\r\n"); // move to end of line
      while (strchr(" \t",*--b)); // trim trailing blanks
      if (!*++b) break; // end
      *b = '\0';
      ++b;
      if (v1!=v2) memmove(v1,v2,b-v2); // move value next to key
      m.push_back(a);
      a = b;
    }
    std::sort(m.begin(),m.end(),chars_less{});
  }
} const mimes_dict("config/mimes");

}

const char* mimes(const char* ext) const noexcept {
  const auto end = mimes_dict.m.end();
  const auto it = std::lower_bound(mimes_dict.m.begin(),end,chars_less{});
  if (it==end || chars_less{}(ext,*it)) return nullptr;
  return *it;
}

const std::map<int,const char*> status_codes {
  {400,"HTTP/1.1 400 Bad Request\r\n\r\n"},
  {401,"HTTP/1.1 401 Unauthorized\r\n\r\n"},
  {403,"HTTP/1.1 403 Forbidden\r\n\r\n"},
  {404,"HTTP/1.1 404 Not Found\r\n\r\n"},
  {405,"HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n"},
  {411,"HTTP/1.1 411 Length Required\r\n\r\n"},
  {413,"HTTP/1.1 413 Payload Too Large\r\n\r\n"},
  {500,"HTTP/1.1 500 Internal Server Error\r\n\r\n"},
  {501,"HTTP/1.1 501 Not Implemented\r\n\r\n"}
};

std::string header(
  std::string_view mime, size_t len, std::string_view more
) {
  return cat(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", std::to_string(len), "\r\n",
    more, "\r\n");
}

request::request(
  const socket sock, char* buffer, size_t size
) {
  INFO("35;1","Reading socket ",sock)
  const auto nread = sock.read(buffer,size);

  if (nread == 0) return;

  INFO("35;1","Parsing HTTP header")
  char *a=buffer, *b=a, *d;
  int nr=0, nn=0;
  for (size_t n=0; ; ++b, ++n) {
    if (n==size) HTTP_ERROR(400,
      "HTTP header: exceeded buffer length: ",size);
    char c = *b;
    if (c=='\r' || c=='\n') { // end of line
      *b = '\0';
      if (c=='\r') {
        if (nr==0) nr = 1;
        else HTTP_ERROR(400,"HTTP header: \\r not followed by \\n");
      } else {
        nr = 0;
        if (++nn == 2) { ++b; break; } // end of header

        // parse line
        if (a==buffer) { // first line -----------------------------------
          d = a;
          while (*d!=' ') {
            if (*d=='\0') HTTP_ERROR(400,"HTTP header: bad header");
            ++d;
          }
          *d = '\0';
          method = a;
          a = d+1;

          d = b;
          while (*d=='\0') --d;
          while (*d!=' ') {
            if (*d=='\0') HTTP_ERROR(400,"HTTP header: bad header");
            --d;
          }
          *d = '\0';
          path = a;
          if (*path!='/') HTTP_ERROR(400,
            "HTTP header: path doesn't start with /");
          protocol = d+1;
        } else { // header ------------------------------------------
          d = a;
          while (*d!=':') {
            if (*d=='\0') HTTP_ERROR(400,
              "HTTP header: field line without \':\'");
            ++d;
          }
          *d = '\0';
          ++d;
          while (*d==' ') ++d;
          header.emplace(a,d);
        }

        a = b+1; // beginning of next line
      }
    } else if (c<'\x20' || '\x7E'<c) {
      HTTP_ERROR(400,"HTTP header: invalid character ",int(c));
    } else {
      nn = 0;
    }
  }

  // get POST data --------------------------------------------------
  const size_t nread_data = nread-(b-buffer);

  if (nread < size) { // default buffer was enough
    data = { b, nread_data };
  } else { // need to allocated a larger buffer
    if (!strcmp(method,"POST")) { // is POST
      const auto [length_it,length_end] = operator[]("Content-Length");
      const auto nlength = std::distance(length_it,length_end);
      if (nlength!=1) {
        if (nlength!=0) {
          HTTP_ERROR(411,"POST request: missing Content-Length");
        } else {
          HTTP_ERROR(411,"POST request: more than one Content-Length");
        }
      } else {
        const size_t length = atol(length_it->second);
        if (length < nread_data) { // claimed length too short
          HTTP_ERROR(413,"POST request: Content-Length < nread_data");
        } else if (length > own_buffer_max_size) { // longer than max
          HTTP_ERROR(413,"POST request: Content-Length > max_size");
        } else if (length == nread_data) { // exact length
          data = { b, nread_data };
        } else { // allocate more space
          own_buffer = reinterpret_cast<char*>(
            memcpy(malloc(length), b, nread_data)
          );
          sock.read(own_buffer+nread_data, length-nread_data);
          data = { own_buffer, length };
        }
      }
    } else HTTP_ERROR(413,method," request: nread > size");
  }
}

float request::qvalue(const char* field, std::string_view value) const {
  for (auto [it,end] = operator[](field); it!=end; ++it) {
    const char *a = it->second, *b, *q;
    for (char c=*a; c!='\0'; c=*(a=b), ++a) {
      while ((c=*a)==' ' || c=='\t') ++a;
      b = a;
      q = nullptr;
      while ((c=*b)!='\0' && c!=',') {
        if (c==';') q = b;
        ++b;
      }
      const char* b2 = q ? q : b;
      if (b2 > a) {
        while ((c=*--b2)==' ' || c=='\t') { }
        ++b2;
        if (std::string_view(a,b2-a) != value) continue;
        if (!q) return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c!='q') return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c!='=') return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c=='\0' || c==',') return 1;
        return atof(q);
      }
    }
  }
  return 0;
}

// parse URL GET parameters -----------------------------------------
form_data::form_data(std::string_view str, bool q) noexcept: mem(str) {
  char* a = mem.data();
  if (q) {
    while (*a!='?' && *a!='\0') ++a;
    if (*a=='?') {
      *a = '\0';
      ++a;
    } else return;
  }
  char* b = a;
  const char** val = nullptr;
  for (char c; ; ++b) {
    c = *b;
    if (c=='&' || (c=='=' && !val) || c=='\0') {
      *b = '\0';
      if (c=='=') val = &params[a];
      else {
        if (val) {
          *val = a;
          val = nullptr;
        } else params[a];
        if (c=='\0') break;
      }
      a = b+1;
    } // end if(=&0)
  }
}

size_t read_buffer_size = 1 << 24;

void send_file(socket sock, const char* name, bool gz) {
  const char *ext = strrchr(name,'.'),
             *mime = "text/plain; charset=UTF-8";
  gz = gz && [ext](const auto*... x){
    return ( strcmp(ext,x) && ... );
  }("jpg","png","webp","gif");
  try {
    const auto cf = file_cache(name,gz);
    const auto header = http::header(mime, cf.size,
      cf.gz ? "Content-Encoding: gzip\r\n" : ""),
    if (cf.data || !cf.size) { // send cached file
      sock << cat(header,cf);
    } else { // read and send
      char* const buf = malloc(read_buffer_size);
      scope_guard buffer_free([buf]{ free(buf); });
      gz = false; // TODO: gzip
      if (gz) {
      } else {
        // TODO: try again with sendfile()
        // https://stackoverflow.com/q/1936037/2640636
        size_t unread = cf.size;
        memcpy(buf,header.data(),header.size());
        size_t nread = PCALLR(read)(cf.fd, buf+header.size(),
          std::min(unread,read_buffer_size-header.size()));
        unread -= nread;
        sock.write(buf,header.size()+nread);
        while (unread) {
          nread = PCALLR(read)(cf.fd, buf, std::min(unread,read_buffer_size));
          unread -= nread;
          sock.write(buf,nread);
        }
      }
    }
  } catch (const std::exception& e) {
    HTTP_ERROR(404,"file ",name,":\n",e.what());
  }
}

void send_str( // TODO: rework
  socket s, std::string_view str, std::string_view mime, bool gz
) {
  if (!mime.empty()) {
    const auto it = mimes.find(mime);
    if (it != mimes.end()) mime = it->second;
    else mime = "text/plain; charset=UTF-8";
  } else mime = "text/plain; charset=UTF-8";

  if (gzok) {
    const auto gz = zlib::deflate_s(str);
    s << cat(
      http::header(mime,gz.size(),"Content-Encoding: gzip\r\n"),
      std::string_view(gz.data(),gz.size()) );
  } else {
    s << cat(http::header(mime,str.size()), str);
  }
}

}
