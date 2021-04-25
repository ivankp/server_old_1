#include <iostream>
#include <fstream>
#include <mutex>
#include <shared_mutex>

#include "whole_file.hh"
#include "server/server.hh"
#include "server/http.hh"
#include "server/websocket.hh"
#include "server/users.hh"
#include "error.hh"
#include "debug.hh"

using namespace ivanp;
using std::cout;

std::shared_mutex mx_users;
const users_table users("db/users");

std::string cookie_login(const http::request& req) {
  std::shared_lock lock(mx_users);
  for (const char* cookie : req["Cookie"])
    if (!strncmp(cookie,"login=",6)) {
      cookie += 6;
      if (strlen(cookie)==users_table::cookie_len) {
        const char* const user = users.cookie_login(cookie);
        if (user) return user;
      }
      break;
    }
  return { };
};
std::string pw_login(const char* name, const char* pw) {
  std::shared_lock lock(mx_users);
  const char* const user = users.pw_login(name,pw);
  if (user) return std::string( get_cookie(user) );
  else return { };
}

int main(int argc, char* argv[]) {
  const server::port_t server_port = 8080;
  const unsigned nthreads = std::thread::hardware_concurrency();
  const unsigned epoll_nevents = 64;
  const size_t thread_buffer_size = 1<<13;

  server server(server_port,epoll_nevents);
  cout << "Listening on port " << server_port <<'\n'<< std::endl;

  server(nthreads, thread_buffer_size,
  [](auto& server, file_desc sock, auto& buffer){
    // HTTP *********************************************************
    if (server.accept(sock)) { try {
      INFO("35;1","HTTP");
      http::request req(sock, buffer.data(), buffer.size(), 1<<20);
      if (!req.method) return;
      const auto g = req.get_params();

#ifndef NDEBUG
      cout << req.method << '\n' << req.path << '\n' << req.protocol << '\n';
      for (const auto& [key, val]: req.header)
        cout << key << ": " << val << '\n';
      cout << '\n';
      cout << g.path() << '\n';
      for (const auto& [key, val]: g.params)
        cout << (key ? key : "NULL") << ": " << (val ? val : "NULL") << '\n';
      cout << std::endl;
#endif

      const char* path = g.path()+1;
      if (!strcmp(req.method,"GET")) { // ===========================
        if (*path=='\0') { // serve index page ----------------------
          const auto user = cookie_login(req);
          if (user.empty()) { // not logged in
            http::send_file(sock,"pages/index.html");
          } else { // logged in
            TEST(user)
            auto page = whole_file("pages/index_user.html");
            { static constexpr char token[] = "<!-- GLOBAL_VARS_JS -->";
              page.replace(page.find(token),sizeof(token)-1,cat(
                "\nconst user = \"",user,"\";\n"
              ));
            }
            { static constexpr char token[] = "<!-- USER_NAME -->";
              page.replace(page.find(token),sizeof(token)-1,user);
            }
            sock <<
              (http::header("text/html; charset=UTF-8",page.size()) + page);
          }
        } else if (!strcmp(path,"chat")) { // initiate websocket ----
          // const auto user = cookie_login(req); // require login
          websocket::handshake(sock, req);
          server.epoll_add(std::move(sock)); // move prevents closing
        } else { // serve any allowed file --------------------------
          // disallow arbitrary path
          for (const char* p=path; ; ++p) {
            if (const char c = *p) {
              // allow only - . / _ 09 AZ az
              if (!( ('-'<=c && c<='9') || ('A'<=c && c<='Z')
                  || c=='_' || ('a'<=c && c<='z') )) {
                HTTP_ERROR(403,
                  "path \"",path,"\" contains a disallowed character "
                  "\'",c,"\'");
              } else
              // disallow ..
              if (c=='.' && (p==path || *(p-1)=='/')) {
                while (*++p=='.') { }
                if (*p=='/' || *p=='\0') {
                  HTTP_ERROR(403,
                    "path \"",path,"\" contains a disallowed sequence \"",
                    std::string_view((p==path ? p : p-1),(*p ? p+1 : p)),"\"");
                } else --p;
              }
            } else break;
          }
          // serve a file
          http::send_file(sock,cat("files/",path));
        }
      } else if (!strcmp(req.method,"POST")) { // ===================
        if (!strcmp(path,"login")) {
          if (req.data.empty()) { // Logout
            sock <<
              "HTTP/1.1 303 See Other\r\n"
              "Location: /\r\n"
              "Set-Cookie: login=0"
                "; Path=/"
                "; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
              "Connection: close\r\n\r\n";
            INFO("32","logout");
          } else { // Login
            const auto form = http::form_data(req.data,!'?');
            const char* name = form["username"];
            const auto cookie = pw_login(name,form["password"]);
            if (!cookie.empty()) {
              sock << cat(
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Set-Cookie: login=", cookie,
                  "; Max-Age=2147483647"
                  "; Path=/\r\n"
                "Connection: close\r\n\r\n"
              );
              INFO("32","logged in user ",name);
            } else {
              sock <<
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Connection: close\r\n\r\n";
              INFO("31","failed to log in user ",name);
            }
          }
        } else {
          HTTP_ERROR(400, "POST with unexpected path \"",path,'\"');
        }
      }

      if (sock.is_valid()) sock.close(); // always close HTTP socket
    } catch (const http::error& e) {
      e.respond(sock);
      sock.close();
      throw;
    } catch (...) {
      // can't make this a function-wide try block
      // because then the main socket might be closed if accept throws
      sock.close();
      throw;
    }
    // WebSocket ****************************************************
    } else { try {
      INFO("35;1","WebSocket")
      auto frame = websocket::receive_frame(sock,buffer.data(),buffer.size());
      if (!frame.data()) return;
      TEST(frame)
      websocket::send_frame(sock,buffer.data(),buffer.size(),
        "TEST");
    } catch (...) {
      // TODO: send response
      sock.close(); // no need to manually remove from epoll
      throw;
    }
    }
    // ******************************************************************
  });

  server.loop();
}
