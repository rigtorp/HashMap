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
#include "HashMap2.h"
#include <chrono>
#include <google/dense_hash_map>
#include <iostream>
#include <random>
#include <unordered_map>

using namespace rigtorp;
using namespace std::chrono;

struct Hash {
  size_t operator()(uint64_t h) const noexcept {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
  }
};

int main(int argc, char *argv[]) {
  (void)argc, (void)argv;

  constexpr size_t count = (1 << 19);
  constexpr size_t iters = 1000;

  using value = std::array<char, 8>;

  volatile size_t out = 0;

  auto b = [count, iters, &out](const char *n, auto &m) {
    // std::mt19937 mt;
    // std::uniform_int_distribution<int> ud(2, count);

    for (size_t i = 0; i < count; ++i) {
      const int val = i; // ud(mt);
      m.insert({val, {}});
    }

    for (size_t i = 0; i < count; ++i) {
      const int val = i; // ud(mt);
      m.erase(val);
    }

    for (size_t i = 0; i < count; ++i) {
      const int val = i; // ud(mt);
      m.insert({val * 2, {}});
    }

    std::cout << m.load_factor() << std::endl;

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      for (size_t j = 0; j < count; ++j) {
        auto res = m.count(j);
        out += res;
        // if (!res) {
        //  std::cout << "bajs " << j << std::endl;
        // return;
        //}
      }
      // const int val = ud(mt);
      // m.erase(val);
      // const int val2 = ud(mt);
      // m.insert({val2, val2});
    }
    auto stop = high_resolution_clock::now();
    auto duration = stop - start;

    std::cout << n << ": "
              << duration_cast<nanoseconds>(duration).count() / (iters * count)
              << " ns/iter" << std::endl;
  };

  {
    HashMap<int, value, Hash> hm(count, INT32_MIN);
    b("HashMap", hm);
    if (out != iters * count) {
      std::cout << "error" << std::endl;
    }
  }

  {
    out = 0;
    HashMap2<int, value, Hash> hm(count);
    b("HashMap2", hm);
    if (out != iters * count) {
      std::cout << "error" << std::endl;
    }
  }

  {
    out = 0;
    google::dense_hash_map<int, value, Hash> hm(count);
    hm.set_empty_key(INT32_MAX);
    hm.set_deleted_key(INT32_MIN + 1);
    b("google::dense_hash_map", hm);
    if (out != iters * count) {
      std::cout << "error" << std::endl;
    }
  }

  {
    out = 0;
    std::unordered_map<int, int, Hash> hm(count);
    b("std::unordered_map", hm);
    if (out != iters * count) {
      std::cout << "error" << std::endl;
    }
  }

  return 0;
}
