// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

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

namespace {

inline uint64_t splitmix64(uint64_t &state) noexcept {
  state += UINT64_C(0x9e3779b97f4a7c15);
  uint64_t z = state;
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}

inline size_t fast_range(uint64_t r, size_t n) noexcept {
#if defined(__SIZEOF_INT128__)
  __extension__ using uint128_t = unsigned __int128;
  return static_cast<size_t>((static_cast<uint128_t>(r) * n) >> 64);
#else
  return static_cast<size_t>(r % n);
#endif
}

template <typename Fn>
void run_timed(const char *workload, const char *name, size_t ops, Fn &&fn) {
  auto start = steady_clock::now();
  fn();
  auto stop = steady_clock::now();
  auto ns = duration_cast<nanoseconds>(stop - start).count();
  std::cout << workload << " / " << name << ": "
            << ns / static_cast<nanoseconds::rep>(ops) << " ns/op" << std::endl;
}

} // namespace

int main(int argc, char *argv[]) {
  size_t count = 10000000;
  size_t iters = 100000000;
  int type = -1;
  std::string workload = "all";

  int opt;
  while ((opt = getopt(argc, argv, "i:c:t:w:")) != -1) {
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
    case 'w':
      workload = optarg;
      break;
    default:
      goto usage;
    }
  }

  if (optind != argc ||
      (workload != "all" && workload != "churn" &&
       workload != "hit-throughput" && workload != "hit-latency" &&
       workload != "miss" && workload != "insert")) {
  usage:
    std::cerr << "HashMapBenchmark © 2020 Erik Rigtorp <erik@rigtorp.se>\n"
                 "usage: HashMapBenchmark [-c count] [-i iters] [-t "
                 "1|2|3|4|5|6] [-w all|churn|hit-throughput|hit-latency|miss|"
                 "insert]\n"
              << std::endl;
    exit(1);
  }

  using key = size_t;
  struct value {
    size_t next;
    char buf[16];
  };

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
  struct hash {
    size_t operator()(size_t h) const noexcept {
      return rigtorp::detail::mix_hash(std::hash<key>{}(h));
    }
  };
#endif

  // Precompute a random Hamiltonian cycle over [2, count + 1] for
  // hit-latency and insert benchmarks.
  std::vector<key> order(count);
  std::iota(order.begin(), order.end(), key{2});
  {
    uint64_t rng = 0;
    for (size_t i = count; i > 1; --i) {
      const size_t j = fast_range(splitmix64(rng), i);
      std::swap(order[i - 1], order[j]);
    }
  }

  auto bench_map = [&](const char *name, auto make_map) {
    if (workload == "all" || workload == "churn") {
      auto m = make_map(count);
      uint64_t rng = 0;
      for (size_t i = 0; i < count; ++i) {
        const key k = 2 + fast_range(splitmix64(rng), count);
        m.insert({k, {k, {}}});
      }
      run_timed("churn", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const key k = 2 + fast_range(splitmix64(rng), count);
          const auto it = m.find(k);
          if (it == m.end()) {
            m.insert({k, {k, {}}});
          } else {
            m.erase(it);
          }
        }
      });
    }

    if (workload == "all" || workload == "hit-throughput") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      uint64_t rng = 0;
      size_t sum = 0;
      run_timed("hit-throughput", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const key k = 2 + fast_range(splitmix64(rng), count);
          sum += m.find(k)->second.next;
        }
      });
      volatile size_t sink = sum;
      (void)sink;
    }

    if (workload == "all" || workload == "hit-latency") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        const key next_k = order[(i + 1) % count];
        m.insert({order[i], {next_k, {}}});
      }
      key k = order[0];
      run_timed("hit-latency", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          k = m.find(k)->second.next;
        }
      });
      volatile size_t sink = k;
      (void)sink;
    }

    if (workload == "all" || workload == "miss") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      uint64_t rng = 0;
      size_t hits = 0;
      run_timed("miss", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const key k = count + 2 + fast_range(splitmix64(rng), count);
          if (m.find(k) != m.end()) {
            ++hits;
          }
        }
      });
      volatile size_t sink = hits;
      (void)sink;
    }

    if (workload == "all" || workload == "insert") {
      auto m = make_map(0);
      run_timed("insert", name, count, [&] {
        for (size_t i = 0; i < count; ++i) {
          m.insert({order[i], {order[i], {}}});
        }
      });
    }
  };

  if (type == -1 || type == 1) {
    bench_map("HashMap", [](size_t reserve_count) {
      HashMap<key, value, std::hash<key>, std::equal_to<>,
              huge_page_allocator<std::pair<key, value>>>
          hm(2, 0);
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
  if (type == -1 || type == 2) {
    bench_map("google::dense_hash_map", [](size_t reserve_count) {
      // Couldn't get it to work with the huge_page_allocator
      google::dense_hash_map<key, value, hash> hm(reserve_count);
      hm.set_empty_key(0);
      hm.set_deleted_key(1);
      return hm;
    });
  }
#endif

#if defined(HASHMAP_BENCHMARK_HAS_ABSL)
  if (type == -1 || type == 3) {
    bench_map("absl::flat_hash_map", [](size_t reserve_count) {
      absl::flat_hash_map<key, value, absl::Hash<key>, std::equal_to<>,
                          huge_page_allocator<std::pair<const key, value>>>
          hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }
#endif

  if (type == -1 || type == 4) {
    bench_map("std::unordered_map", [](size_t reserve_count) {
      std::unordered_map<key, value> hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }

#if defined(HASHMAP_BENCHMARK_HAS_BOOST)
  if (type == -1 || type == 5) {
    bench_map("boost::unordered_flat_map", [](size_t reserve_count) {
      boost::unordered_flat_map<
          key, value, boost::hash<key>, std::equal_to<>,
          huge_page_allocator<std::pair<const key, value>>>
          hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }

  if (type == -1 || type == 6) {
    bench_map("boost::unordered_map", [](size_t reserve_count) {
      boost::unordered_map<key, value> hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }
#endif

  return 0;
}
