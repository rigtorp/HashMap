#include <functional>
#include <rigtorp/HashMap.h>

int main() {
  rigtorp::HashMap<int, int> map(0, -1);
  map.emplace(0, 42);
  map.emplace(1, 43);
  map.erase(1);
  return map.size() == 1 && map.at(0) == 42 ? 0 : 1;
}
