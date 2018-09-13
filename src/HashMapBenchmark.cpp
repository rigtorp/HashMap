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

#include <chrono>
#include <google/dense_hash_map>
#include <iostream>
#include <random>
#include <rigtorp/HashMap.h>
#include <unordered_map>

using namespace rigtorp;
using namespace std::chrono;

int main(int argc, char *argv[]) {
  (void)argc, (void)argv;

  constexpr size_t count = 1000;
  constexpr size_t iters = 100000000;

  auto b = [count, iters](const char *n, auto &m) {
    std::mt19937 mt;
    std::uniform_int_distribution<int> ud(2, count);

    for (size_t i = 0; i < count; ++i) {
      const int val = ud(mt);
      m.insert({val, val});
    }

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      const int val = ud(mt);
      m.erase(val);
      const int val2 = ud(mt);
      m.insert({val2, val2});
    }
    auto stop = high_resolution_clock::now();
    auto duration = stop - start;

    std::cout << n << ": "
              << duration_cast<nanoseconds>(duration).count() / iters
              << " ns/iter" << std::endl;
  };

  {
    HashMap<int, int> hm(count, 0);
    b("HashMap", hm);
  }

  {
    google::dense_hash_map<int, int> hm(count);
    hm.set_empty_key(0);
    hm.set_deleted_key(1);
    b("google::dense_hash_map", hm);
  }

  {
    std::unordered_map<int, int> hm(count);
    b("std::unordered_map", hm);
  }

  return 0;
}
