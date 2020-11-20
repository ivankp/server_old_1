#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>

#include "users.hh"
#include "debug.hh"

using std::cout;
using std::endl;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  if (argc!=2) {
    cout << "usage: " << argv[0] << " username\n";
    return 1;
  }
  // TODO: verify username validity

  const fs::path db = "db/users";

  users_table users;

  std::error_code file_err;
  const auto fsize = fs::file_size(db,file_err);

  if (fsize && fsize < decltype(fsize)(-1)) {
    users = users_table(db.c_str());
  } else {
    fs::create_directories(db.parent_path());
  }

  std::mt19937 gen(
    std::chrono::system_clock::now().time_since_epoch().count());

  auto rndstr = [
    &gen, dist = std::uniform_int_distribution<>(0,sizeof(charset)-2)
    // -2 because charset ends in \0
  ](unsigned len) mutable {
    std::string str;
    str.reserve(len);
    std::generate_n(
      std::back_inserter(str), len, [&]{ return charset[dist(gen)]; });
    return str;
  };

  std::string cookie;
  for (;;) { // generate unique cookie
    cookie = rndstr(users_table::cookie_len);
    if (users.cookie_login(cookie)) continue;
    break;
  }
  TEST(cookie)
  const auto pw = rndstr(12);

  std::uniform_int_distribution<char> dist_char(
    std::numeric_limits<char>::min(),
    std::numeric_limits<char>::max()
  );

  char salt[16];
  std::generate_n(salt,sizeof(salt),[&]{ return dist_char(gen); });

  const auto hash = bcrypt_hash(pw.c_str(),salt,sizeof(salt));
  TEST(hash)

#ifndef NDEBUG
  if (!bcrypt_check(pw.c_str(),hash.c_str())) {
    cout << "hash check failed\n";
    return 1;
  }
#endif

  const char* user = users[argv[1]];
  if (!user) {
    user = argv[1];
    std::ofstream(db, std::ios_base::app)
      << hash << cookie << user << '\0';

    cout << "created new user\n";
  } else {
    cout << "updated existing user\n";
  }

  cout << "\nusername: " << user
       << "\npassword: " << pw << '\n';
}

