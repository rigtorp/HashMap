#include "HashMap.h"
#include <iostream>

int main(int argc, char *argv[]) {
  using namespace rigtorp;

  // Create a HashMap with 16 buckets and 0 as the empty key
  HashMap<int, int> hm(16, 0);
  hm.emplace(1, 1);
  hm[2] = 2;

  // Iterate and print key-value pairs
  for (const auto &e : hm) {
    std::cout << e.first << " = " << e.second << "\n";
  }

  // Erase entry
  hm.erase(1);

  return 0;
}
