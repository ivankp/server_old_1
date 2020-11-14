#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <functional>
#include <iterator>
#include <cstdint>

#include "users.hh"
#include "debug.hh"

using std::cout;
using std::endl;
namespace fs = std::filesystem;

constexpr char charset[] =
  "0123456789"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz";

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
    &gen, dist = std::uniform_int_distribution<>(0,sizeof(charset)-1)
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

  std::uniform_int_distribution<uint64_t> dist_uint64(0);

  const auto salt = dist_uint64(gen);
  TEST(salt)

  const std::string_view user = users[argv[1]];
  if (!user.data()) {
    std::ofstream out(db, std::ios_base::app);
    out << cookie;
    out.write(reinterpret_cast<const char*>(&salt),sizeof(salt));
    out << pw << argv[1] << '\0';

    cout << "created new user\n";
  } else {
    cout << "updated existing user\n";
  }

  cout << "\nusername: " << argv[1]
       << "\npassword: " << pw << '\n';
}
