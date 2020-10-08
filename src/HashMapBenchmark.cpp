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

#include <nmmintrin.h> // _mm_crc32_u64

#include <chrono>
#include <iostream>
#include <random>
#include <unistd.h>
#include <unordered_map>

#if __has_include(<google/dense_hash_map>)
#include <google/dense_hash_map>
#endif

#if __has_include(<absl/container/flat_hash_map.h>)
#include <absl/container/flat_hash_map.h>
#endif

#include <rigtorp/HashMap.h>

#if __has_include(<sys/mman.h>)
#include <sys/mman.h> // mmap, munmap
#endif

#if defined(MAP_POPULATE) && defined(MAP_HUGETLB)
template <typename T> struct huge_page_allocator {
  constexpr static std::size_t huge_page_size = 1 << 21; // 2 MiB
  using value_type = T;

  huge_page_allocator() = default;
  template <class U>
  constexpr huge_page_allocator(const huge_page_allocator<U> &) noexcept {}

  size_t round_to_huge_page_size(size_t n) {
    return (((n - 1) / huge_page_size) + 1) * huge_page_size;
  }

  T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::bad_alloc();
    }
    auto p = static_cast<T *>(mmap(
        nullptr, round_to_huge_page_size(n * sizeof(T)), PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, -1, 0));
    if (p == MAP_FAILED) {
      throw std::bad_alloc();
    }
    return p;
  }

  void deallocate(T *p, std::size_t n) {
    munmap(p, round_to_huge_page_size(n));
  }
};
#else
template <typename T> using huge_page_allocator = std::allocator<T>;
#endif

using namespace std::chrono;
using namespace rigtorp;

int main(int argc, char *argv[]) {
  (void)argc, (void)argv;

  size_t count = 10000000;
  size_t iters = 100000000;
  int type = -1;

  int opt;
  while ((opt = getopt(argc, argv, "i:c:t:")) != -1) {
    switch (opt) {
    case 'i':
      iters = std::stol(optarg);
      break;
    case 'c':
      count = std::stol(optarg);
      break;
    case 't':
      type = std::stoi(optarg);
      break;
    default:
      goto usage;
    }
  }

  if (optind != argc) {
  usage:
    std::cerr << "HashMapBenchmark © 2020 Erik Rigtorp <erik@rigtorp.se>\n"
                 "usage: HashMapBenchmark [-c count] [-i iters] [-t 1|2|3|4]\n"
              << std::endl;
    exit(1);
  }

  using key = size_t;
  struct value {
    char buf[24];
  };

  struct hash {
    size_t operator()(size_t h) const noexcept { return _mm_crc32_u64(0, h); }
  };

  auto b = [&](const char *n, auto &m) {
    std::minstd_rand gen(0);
    std::uniform_int_distribution<int> ud(2, count);

    for (size_t i = 0; i < count; ++i) {
      const int val = ud(gen);
      m.insert({val, {}});
    }

    auto start = steady_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      const int val = ud(gen);
      const auto it = m.find(val);
      if (it == m.end()) {
        m.insert({val, {}});
      } else {
        m.erase(it);
      }
    }
    auto stop = steady_clock::now();
    auto duration = stop - start;

    nanoseconds max = {};
    for (size_t i = 0; i < iters; ++i) {
      const int val = ud(gen);
      auto start = steady_clock::now();
      const auto it = m.find(val);
      if (it == m.end()) {
        m.insert({val, {}});
      } else {
        m.erase(it);
      }
      auto stop = steady_clock::now();
      max = std::max(max, stop - start);
    }

    std::cout << n << ": mean "
              << duration_cast<nanoseconds>(duration).count() / iters
              << " ns/iter, max " << max.count() << " ns/iter" << std::endl;
  };

  if (type == -1 || type == 1) {
    HashMap<key, value, hash, std::equal_to<>,
            huge_page_allocator<std::pair<key, value>>>
        hm(2 * count, 0);
    b("HashMap", hm);
  }

#if __has_include(<google/dense_hash_map>)
  if (type == -1 || type == 2) {
    // Couldn't get it to work with the huge_page_allocator
    google::dense_hash_map<key, value, hash> hm(count);
    hm.set_empty_key(0);
    hm.set_deleted_key(1);
    b("google::dense_hash_map", hm);
  }
#endif

#if __has_include(<absl/container/flat_hash_map.h>)
  if (type == -1 || type == 3) {
    absl::flat_hash_map<key, value, hash, std::equal_to<>,
                        huge_page_allocator<std::pair<key, value>>>
        hm;
    hm.reserve(count);
    b("absl::flat_hash_map", hm);
  }
#endif

  if (type == -1 || type == 4) {
    std::unordered_map<key, value, hash> hm;
    hm.reserve(count);
    b("std::unordered_map", hm);
  }

  return 0;
}
