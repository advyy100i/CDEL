#include <cstdio>
#include <string>

static void raw_query(const std::string& sql) {
    std::printf("[SQL] %s\n", sql.c_str());
}

#ifdef ENABLE_ORM
// ORM layer — ENABLE_ORM is OFF and never wired; dead code.
struct User {
    int         id;
    std::string name;
};

static User find_user_by_id(int id) {
    (void)id;
    return {0, "unknown"};
}

static bool save_user(const User& u) {
    std::printf("[ORM] saving user: %s\n", u.name.c_str());
    return true;
}

static void delete_user(int id) {
    std::printf("[ORM] deleting user id=%d\n", id);
}
#endif  // ENABLE_ORM

int main() {
    raw_query("SELECT * FROM users WHERE active = 1");
    return 0;
}
