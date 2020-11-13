#include "users.hh"

#include <fstream>

users_map read_users(const char* file) {
  users_map users;
  std::ifstream f(file);
  std::string cookie;
  while (f >> cookie) {
    auto& user = users[cookie];
    f >> user.id >> user.name;
  }
  return users;
}
