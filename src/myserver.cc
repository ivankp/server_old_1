#include <iostream>
#include <fstream>

#include "server.hh"
#include "http.hh"
#include "websocket.hh"
#include "users.hh"
#include "error.hh"
#include "debug.hh"

using namespace ivanp;
using std::cout;

std::string whole_file(const char* filename) {
  std::ifstream f(filename);
  std::string str;

  f.seekg(0, std::ios::end);
  str.reserve(f.tellg());
  f.seekg(0, std::ios::beg);

  str.assign(std::istreambuf_iterator<char>(f),
             std::istreambuf_iterator<char>());
  return str;
}

users_map users;
const user_def* cookie_login(const http::request& req) {
  for (const char* cookie : req["Cookie"]) {
    if (starts_with(cookie,"login="))
      return find_user(users,cookie+6);
  }
  return nullptr;
};

int main(int argc, char* argv[]) {
  const server::port_t server_port = 8080;
  const unsigned nthreads = std::thread::hardware_concurrency();
  const unsigned epoll_nevents = 64;
  const size_t thread_buffer_size = 1<<13;

  users = read_users("db/users");

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
      const auto user = cookie_login(req);
      if (!strcmp(req.method,"GET")) { // ===========================
        if (*path=='\0') { // serve index page ----------------------
          if (!user) { // not logged in
            http::send_file(sock,"pages/index.html");
          } else { // logged in
            TEST(user->id)
            auto page = whole_file("pages/index_user.html");
            static constexpr char token[] = "<!-- GLOBAL_VARS_JS -->";
            page.replace(page.find(token),sizeof(token)-1,cat(
              "\nconst user = {id:",user->id,",name:\"",user->name,"\"};\n"
            ));
            sock <<
              (http::header("text/html; charset=UTF-8",page.size()) + page);
          }
        } else if (!strcmp(path,"chat")) { // initiate websocket ----
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
          http::send_file(sock,cat("share/",path));
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
            if (user) {
              INFO("32","logged out user ",user->name," (",user->id,")");
            } else {
              INFO("32","logged out nobody");
            }
          } else { // Login
            const auto form_data = http::form_data(req.data,!'?');
            const auto* user = [](const char* name)
            -> const users_map::value_type* {
              for (const auto& user : users)
                if (user.second.name == name) return &user;
              return nullptr;
            }(form_data["username"]);
            if (user) {
              sock << cat(
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Set-Cookie: login=",user->first,
                "; Max-Age=2147483647"
                "; Path=/\r\n"
                "Connection: close\r\n\r\n"
              );
              INFO("32","logged in user ",user->second.name);
            } else {
              sock <<
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Connection: close\r\n\r\n";
              INFO("32","failed to log in user ",form_data["username"]);
            }
          }
        } else {
          HTTP_ERROR(400, "POST with unexpected path \"",path,"\"");
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
