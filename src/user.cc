#include <iostream>

#include "server/users.hh"
#include "debug.hh"

using std::cout;
using std::endl;

int main(int argc, char* argv[]) {
  if (argc!=2) {
    cout << "usage: " << argv[0] << " username\n";
    return 1;
  }
  // TODO: verify username validity

  users_table users("db/users");

  const char* name = argv[1];
  const char* user = users[name];
  const auto pw = rndstr(12);

  if (!user) {
    cout << "creating new user \"" << name << "\"" << endl;
    user = users.new_user(name,pw.c_str());
    if (!user) {
      cout << "failed\n";
      return 1;
    }
  } else {
    cout << "updating existing user \"" << user << "\"" << endl;
    users.reset_pw(user,pw.c_str());
  }

  // TEST(get_cookie(user))

  cout << "\nusername: " << user
       << "\npassword: " << pw << '\n';
}

