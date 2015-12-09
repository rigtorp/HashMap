#include "HashMap.h"
#include <chrono>
#include <google/dense_hash_map>
#include <iostream>
#include <random>
#include <unordered_map>

int main(int argc, char *argv[]) {
  constexpr size_t count = 1000000;
  constexpr size_t iters = 1000000000;
  using namespace std::chrono;

  {
    HashMap<int, int> hm(count, 0);
    std::mt19937 mt;
    std::uniform_int_distribution<int> ud(2, count);

    int val;
    for (size_t i = 0; i < count; ++i) {
      val = ud(mt);
      hm.emplace(val, val);
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      hm.erase(val);
      val = ud(mt);
      hm.emplace(val, val);
    }
    auto stop = high_resolution_clock::now();
    auto duration = stop - start;

    std::cout << "HashMap: "
              << duration_cast<nanoseconds>(duration).count() / iters
              << " ns/iter" << std::endl;
  }

  {
    google::dense_hash_map<int, int> hm(count);
    hm.set_empty_key(0);
    hm.set_deleted_key(1);

    std::mt19937 mt;
    std::uniform_int_distribution<int> ud(2, count);

    int val;
    for (size_t i = 0; i < count; ++i) {
      val = ud(mt);
      hm.insert(std::make_pair(val, val));
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      hm.erase(val);
      val = ud(mt);
      hm.insert(std::make_pair(val, val));
    }
    auto stop = high_resolution_clock::now();
    auto duration = stop - start;

    std::cout << "google::dense_hash_map: "
              << duration_cast<nanoseconds>(duration).count() / iters
              << " ns/iter" << std::endl;
  }

  {
    std::unordered_map<int, int> hm(count);
    std::mt19937 mt;
    std::uniform_int_distribution<int> ud(2, count);

    int val;
    for (size_t i = 0; i < count; ++i) {
      val = ud(mt);
      hm.emplace(val, val);
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      hm.erase(val);
      val = ud(mt);
      hm.emplace(val, val);
    }
    auto stop = high_resolution_clock::now();
    auto duration = stop - start;

    std::cout << "std::unordered_map: "
              << duration_cast<nanoseconds>(duration).count() / iters
              << " ns/iter" << std::endl;
  }

  return 0;
}
