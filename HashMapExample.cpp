#include "HashMap.h"
#include <iostream>

int main(int argc, char *argv[]) {
  (void)argc, (void)argv;

  using namespace rigtorp;

  // Hash for using std::string as lookup key
  struct Hash {
    size_t operator()(int v) { return v * 7; }
    size_t operator()(const std::string &v) { return std::stoi(v) * 7; }
  };

  // Equal comparison for using std::string as lookup key
  struct Equal {
    bool operator()(int lhs, int rhs) { return lhs == rhs; }
    bool operator()(int lhs, const std::string &rhs) {
      return lhs == std::stoi(rhs);
    }
  };

  // Create a HashMap with 16 buckets and 0 as the empty key
  HashMap<int, int, Hash, Equal> hm(16, 0);
  hm.emplace(1, 1);
  hm[2] = 2;

  // Iterate and print key-value pairs
  for (const auto &e : hm) {
    std::cout << e.first << " = " << e.second << "\n";
  }

  // Lookup using std::string
  std::cout << hm.at("1") << "\n";

  // Erase entry
  hm.erase(1);

  return 0;
}
