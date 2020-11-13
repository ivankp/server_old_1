#ifndef IVANP_SERVER_USERS_HH
#define IVANP_SERVER_USERS_HH

#include <map>
#include <string>
#include <functional>

struct user_def {
  std::string name;
  unsigned id;
};

using users_map = std::map<std::string,user_def,std::less<>>;

users_map read_users(const char* file);

inline const user_def* find_user(
  const users_map& users, const char* cookie
) noexcept {
  const auto u = users.find(cookie);
  return u == users.end() ? nullptr : &u->second;
}

#endif
