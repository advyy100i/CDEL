#include <cstdio>
#include <string>

#ifdef NEW_PARSER
// This branch is ALWAYS compiled — NEW_PARSER is ON in every target.
int parse(const std::string& input) {
    std::printf("[new] parsing: %s\n", input.c_str());
    return static_cast<int>(input.size());
}
#else
// This #else branch is NEVER compiled — dead code.
// OLD_PARSER logic from 2018; superseded by NEW_PARSER.
int parse(const std::string& input) {
    std::printf("[old] parsing: %s\n", input.c_str());
    return 0;
}
#endif  // NEW_PARSER

int main() {
    int result = parse("hello");
    std::printf("result: %d\n", result);
    return 0;
}
