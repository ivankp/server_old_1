#include "http.hh"

#include <iostream>
#include <cstdlib>

#include "server/socket.hh"
#include "local_fd.hh"
#include "whole_file.hh"
#include "file_cache.hh"
#include "zlib.hh"
#include "error.hh"
#include "debug.hh"

namespace ivanp::http {

const std::map<const char*,const char*,chars_less> mimes = []{
  static constexpr auto filename = "config/mimes";
  static auto s = whole_file(filename);
  std::map<const char*,const char*,chars_less> mimes;
  for (char *a=s.data(), *b, *c, *const end=a+s.size(); ; ) {
    ctrim(a,end,' ','\t','\n','\0');
    if (a==end) break;
    b = static_cast<char*>(memchr(a,' ',end-a));
    if (!b || b+1==end) ERROR(filename);
    *b = '\0';
    ctrim(++b,end,' ','\t','\n','\0');
    c = static_cast<char*>(memchr(b,'\n',end-b));
    mimes[a] = b;
    if (!c) break;
    *c = '\0';
    a = c+1;
  }
  return mimes;
}();

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

std::string header(
  std::string_view mime, size_t len, std::string_view more
) {
  return cat(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", std::to_string(len), "\r\n",
    more, "\r\n");
}

// send whole file --------------------------------------------------
void send_file(socket sock, const char* name, bool gzok) {
  scope_fd f1 = ::open(name, O_RDONLY);
  struct stat s1;
  if (f1 == -1)
    HTTP_ERROR(404,"open(",name,"): ",std::strerror(errno));
  if (::fstat(f1, &s1) == -1)
    HTTP_ERROR(404,"fstat(): ",std::strerror(errno));
  if (!S_ISREG(s1.st_mode))
    HTTP_ERROR(404,name," is not a regular file");

  const char *mime = "text/plain; charset=UTF-8",
             *ext = strrchr(name,'.');
  if (ext) {
    const auto it = mimes.find(++ext);
    if (it != mimes.end()) mime = it->second;
  }

  // TODO: this is a hack
  if (s1.st_size >= ivanp::file_cache_max_size) {
    sock << header(mime, s1.st_size) << whole_file(name);
    return;
  }

  const char* encoding = "";
  std::string name_gz;

  if (gzok && [ext](const auto*... x){
    return ( ... && strcmp(ext,x) );
  }("jpg","png","webp","gif")){ // exclude extensions
    name_gz = cat("cache/gz/",name,".gz");
    scope_fd f2 = ::open(name_gz.c_str(), O_RDONLY);
    struct stat s2;
    if (f2 != -1) {
      if (::fstat(f2, &s2) == -1) {
        REDERR "fstat(): " << std::strerror(errno);
      } else if (!S_ISREG(s2.st_mode)) {
        REDERR << name_gz << " is not a regular file";
      } else if (s1.st_mtime < s2.st_mtime) { // send gz file from cache
        goto send_gz;
      } else { // gz file needs updating
        if (::unlink(name_gz.c_str()) == -1)
          REDERR "unlink(): " << std::strerror(errno)
            << "\033[0m" << std::endl;
        goto update_gz;
      }
      std::cerr << "\033[0m" << std::endl;
    } else { // create gz file
      for (char* p=name_gz.data(); *p && (p=strchr(p,'/')); ++p) {
        *p = '\0';
        if (::mkdir(name_gz.data(),0755) == -1 && errno != EEXIST) {
          REDERR "mkdir(): " << std::strerror(errno) << "\033[0m" << std::endl;
          goto failed_gz;
        }
        *p = '/';
      }
update_gz:
      if ((f2 = ::open(name_gz.c_str(), O_CREAT|O_RDWR|O_TRUNC, 0644)) != -1) {
        try {
          s2.st_size = zlib::deflate_f(f1,f2,s1.st_size);
          PCALL(lseek)(f2,0,SEEK_SET);
        } catch (const std::exception& e) {
          std::cerr << "\033[31m" << e.what() << "\033[0m" << std::endl;
          f2 = { };
        }
        if (f2 != -1) {
send_gz:
          // f1 = std::move(f2); // closes f1
          s1.st_size = s2.st_size;
          name = name_gz.c_str();
          encoding = "Content-Encoding: gzip\r\n";
        }
      }
    }
failed_gz: ;
  }

  // TODO: TCP_CORK
  if (const auto f = file_cache(name)) {
    sock << header(mime, s1.st_size, encoding) << *f;
  } else {
    // TODO: send file without caching
    HTTP_ERROR(500,name," cannot be cached");
  }
  // { off_t offset = 0;
  //   size_t size = s1.st_size;
  //   while (size) {
  //     const auto ret = ::sendfile(sock, f1, &offset, size);
  //     if (ret < 0) {
  //       if (errno == EAGAIN || errno == EWOULDBLOCK) {
  //         // ::sched_yield();
  //         std::this_thread::yield();
  //         continue;
  //       } else THROW_ERRNO("sendfile()");
  //     } else if (ret == 0) break; // NOTE: zero return means done
  //     offset += ret;
  //     size -= ret;
  //   }
  // }
}

void send_str(
  socket fd, std::string_view str, std::string_view mime, bool gzok
) {
  if (!mime.empty()) {
    const auto it = mimes.find(mime);
    if (it != mimes.end()) mime = it->second;
    else mime = "text/plain; charset=UTF-8";
  } else mime = "text/plain; charset=UTF-8";

  if (gzok) {
    const auto gz = zlib::deflate_s(str);
    fd << cat(
      http::header(mime,gz.size(),"Content-Encoding: gzip\r\n"),
      std::string_view(gz.data(),gz.size()) );
  } else {
    fd << cat(http::header(mime,str.size()), str);
  }
}

}
