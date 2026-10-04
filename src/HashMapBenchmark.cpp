// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

#if defined(__x86_64__) || defined(_M_X64)
#include <nmmintrin.h> // _mm_crc32_u64
#endif

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <unistd.h>
#include <unordered_map>

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
#include <google/dense_hash_map>
#endif

#if defined(HASHMAP_BENCHMARK_HAS_ABSL)
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
    munmap(p, round_to_huge_page_size(n * sizeof(T)));
  }
};

template <typename T, typename U>
bool operator==(const huge_page_allocator<T> &,
                const huge_page_allocator<U> &) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const huge_page_allocator<T> &,
                const huge_page_allocator<U> &) noexcept {
  return false;
}
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
    size_t operator()(size_t h) const noexcept {
#if defined(__x86_64__) || defined(_M_X64)
      return _mm_crc32_u64(0, h);
#else
      // MurmurHash3's public-domain fmix64 finalizer, by Austin Appleby:
      // https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
      uint64_t x = h;
      x ^= x >> 33;
      x *= UINT64_C(0xff51afd7ed558ccd);
      x ^= x >> 33;
      x *= UINT64_C(0xc4ceb9fe1a85ec53);
      x ^= x >> 33;
      return static_cast<size_t>(x);
#endif
    }
  };

  auto b = [&](const char *n, auto &m) {
    std::minstd_rand gen(0);
    std::uniform_int_distribution<key> ud(2, count);

    for (size_t i = 0; i < count; ++i) {
      const key val = ud(gen);
      m.insert({val, {}});
    }

    auto start = steady_clock::now();
    for (size_t i = 0; i < iters; ++i) {
      const key val = ud(gen);
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
      const key val = ud(gen);
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

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
  if (type == -1 || type == 2) {
    // Couldn't get it to work with the huge_page_allocator
    google::dense_hash_map<key, value, hash> hm(count);
    hm.set_empty_key(0);
    hm.set_deleted_key(1);
    b("google::dense_hash_map", hm);
  }
#endif

#if defined(HASHMAP_BENCHMARK_HAS_ABSL)
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
