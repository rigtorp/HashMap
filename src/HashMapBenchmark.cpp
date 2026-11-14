// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

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

#if defined(HASHMAP_BENCHMARK_HAS_BOOST)
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered_map.hpp>
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
      iters = std::stoul(optarg);
      break;
    case 'c':
      count = std::stoul(optarg);
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
    std::cerr
        << "HashMapBenchmark © 2020 Erik Rigtorp <erik@rigtorp.se>\n"
           "usage: HashMapBenchmark [-c count] [-i iters] [-t 1|2|3|4|5|6]\n"
        << std::endl;
    exit(1);
  }

  using key = size_t;
  struct value {
    char buf[24];
  };

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
  struct hash {
    size_t operator()(size_t h) const noexcept {
      return rigtorp::detail::mix_hash(std::hash<key>{}(h));
    }
  };
#endif

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
      auto iter_start = steady_clock::now();
      const auto it = m.find(val);
      if (it == m.end()) {
        m.insert({val, {}});
      } else {
        m.erase(it);
      }
      auto iter_stop = steady_clock::now();
      max = std::max(max, iter_stop - iter_start);
    }

    std::cout << n << ": mean "
              << duration_cast<nanoseconds>(duration).count() /
                     static_cast<nanoseconds::rep>(iters)
              << " ns/iter, max " << max.count() << " ns/iter" << std::endl;
  };

  if (type == -1 || type == 1) {
    HashMap<key, value, std::hash<key>, std::equal_to<>,
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
    absl::flat_hash_map<key, value, absl::Hash<key>, std::equal_to<>,
                        huge_page_allocator<std::pair<const key, value>>>
        hm;
    hm.reserve(count);
    b("absl::flat_hash_map", hm);
  }
#endif

  if (type == -1 || type == 4) {
    std::unordered_map<key, value> hm;
    hm.reserve(count);
    b("std::unordered_map", hm);
  }

#if defined(HASHMAP_BENCHMARK_HAS_BOOST)
  if (type == -1 || type == 5) {
    boost::unordered_flat_map<key, value, boost::hash<key>, std::equal_to<>,
                              huge_page_allocator<std::pair<const key, value>>>
        hm;
    hm.reserve(count);
    b("boost::unordered_flat_map", hm);
  }

  if (type == -1 || type == 6) {
    boost::unordered_map<key, value> hm;
    hm.reserve(count);
    b("boost::unordered_map", hm);
  }
#endif

  return 0;
}
