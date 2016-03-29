/*
Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include "HashMap.h"
#include <chrono>
#include <google/dense_hash_map>
#include <iostream>
#include <random>
#include <unordered_map>

using namespace rigtorp;
using namespace std::chrono;

int main(int argc, char *argv[]) {
  constexpr size_t count = 1000000;
  constexpr size_t iters = 1000000000;

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
