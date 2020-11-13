#include "http.hh"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "debug.hh"

namespace ivanp::http {

const std::map<const char*,const char*,chars_less> memes {
  {"html","text/html; charset=UTF-8"},
  {"ico","image/x-icon"},
  {"css","text/css"},
  {"js","application/javascript"},
  {"json","application/json"},
  {"txt","text/plain; charset=UTF-8"}
};

const std::map<int,const char*> status_codes {
  {400,"HTTP/1.1 400 Bad Request\r\n\r\n"},
  {403,"HTTP/1.1 403 Forbidden\r\n\r\n"},
  {404,"HTTP/1.1 404 Not Found\r\n\r\n"},
  {405,"HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n"},
  {411,"HTTP/1.1 411 Length Required\r\n\r\n"},
  {413,"HTTP/1.1 413 Payload Too Large\r\n\r\n"},
  {500,"HTTP/1.1 500 Internal Server Error\r\n\r\n"}
};

void error::respond(file_desc& sock) const {
  sock << status_codes.at(code);
}

request::request(
  const file_desc& sock, char* buffer, size_t size, size_t max_size
) {
  INFO("35;1","Reading socket ",*sock)
  const auto nread = sock.read(buffer,size);
  if (nread == 0) return;

  INFO("35;1","Parsing HTTP header")
  char *a=buffer, *b=buffer, *d;
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
          if (path[0]!='/') HTTP_ERROR(400,
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
      HTTP_ERROR(400,"HTTP header: invalid character \'",int(c),'\'');
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
      const auto length_r = operator[]("Content-Length");
      if (length_r.size()!=1) {
        if (length_r.size()!=0) {
          HTTP_ERROR(411,"POST request: missing Content-Length");
        } else {
          HTTP_ERROR(411,"POST request: more than one Content-Length");
        }
      } else {
        const size_t length = atol(*length_r);
        if (length < nread_data) { // claimed length too short
          HTTP_ERROR(413,"POST request: Content-Length < nread_data");
        } else if (length > max_size) { // longer than max
          HTTP_ERROR(413,"POST request: Content-Length > max_size");
        } else if (length == nread_data) { // exact length
          data = { b, nread_data };
        } else { // allocate more space
          char* m = mem(length);
          memcpy(m, b, nread_data);
          sock.read(m+nread_data, length-nread_data);
          data = { m, length };
        }
      }
    } else HTTP_ERROR(413,method," request: nread > size");
  }

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
  std::string_view meme, size_t len, std::string_view more
) {
  return cat(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: ", meme, "\r\n"
    "Content-Length: ", std::to_string(len), "\r\n"
    "Connection: close\r\n",
    more, "\r\n");
}

// send whole file --------------------------------------------------
void send_file(
  file_desc& fd, std::string in_name,
  std::string_view meme, std::string_view more
) {
  try {
    in_name += ".gz";
    const bool gz = ::access(in_name.c_str(), R_OK) != -1;
    if (!gz) in_name.resize(in_name.size()-3);

    int in_fd = ::open(in_name.c_str(), O_RDONLY);
    if (in_fd == -1) THROW_ERRNO("open()");
    if (gz) in_name.resize(in_name.size()-3);

    struct stat sb;
    if (::fstat(in_fd, &sb) == -1) THROW_ERRNO("fstat()");
    if (!S_ISREG(sb.st_mode)) THROW_ERRNO("not a regular file");
    const auto len = sb.st_size;

    if (meme.empty()) {
      const char* ext = strrchr(in_name.c_str(),'.');
      if (!ext) meme = "text/plain; charset=UTF-8";
      else {
        try {
          meme = memes.at(ext+1);
        } catch (...) {
          meme = "text/plain; charset=UTF-8";
        }
      }
    }

    fd << header(meme,len,cat(
      (gz ? "Content-Encoding: gzip\r\n" : ""),
      more
    ));

    if (::sendfile(*fd, in_fd, nullptr, len) == -1) THROW_ERRNO("sendfile()");
    if (::close(in_fd) == -1) THROW_ERRNO("close()");

  } catch (const std::exception& e) {
    throw http::error(500,"send_file(",in_name,"): ",e.what());
  }
}

}
