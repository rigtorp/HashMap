// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
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

inline size_t g_alloc_bytes = 0;
inline size_t g_peak_bytes = 0;

struct memory_scope {
  memory_scope() noexcept {
    g_alloc_bytes = 0;
    g_peak_bytes = 0;
  }
  static size_t current() noexcept { return g_alloc_bytes; }
  static size_t peak() noexcept { return g_peak_bytes; }
};

template <typename T, template <typename> class BaseAlloc>
struct tracking_allocator {
  using value_type = T;
  using is_always_equal = std::true_type;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template <typename U> struct rebind {
    using other = tracking_allocator<U, BaseAlloc>;
  };

  tracking_allocator() = default;
  template <typename U>
  constexpr tracking_allocator(
      const tracking_allocator<U, BaseAlloc> &) noexcept {}

  T *allocate(std::size_t n) {
    T *p = BaseAlloc<T>{}.allocate(n);
    g_alloc_bytes += n * sizeof(T);
    if (g_alloc_bytes > g_peak_bytes) {
      g_peak_bytes = g_alloc_bytes;
    }
    return p;
  }

  void deallocate(T *p, std::size_t n) noexcept {
    g_alloc_bytes -= n * sizeof(T);
    BaseAlloc<T>{}.deallocate(p, n);
  }
};

template <typename T, typename U, template <typename> class BaseAlloc>
bool operator==(const tracking_allocator<T, BaseAlloc> &,
                const tracking_allocator<U, BaseAlloc> &) noexcept {
  return true;
}

template <typename T, typename U, template <typename> class BaseAlloc>
bool operator!=(const tracking_allocator<T, BaseAlloc> &,
                const tracking_allocator<U, BaseAlloc> &) noexcept {
  return false;
}

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
void run_timed(const std::string &prefix, const char *workload,
               const char *name, size_t ops, Fn &&fn) {
  auto start = steady_clock::now();
  fn();
  auto stop = steady_clock::now();
  auto ns = duration_cast<nanoseconds>(stop - start).count();
  std::cout << prefix << workload << " / " << name << ": "
            << ns / static_cast<nanoseconds::rep>(ops) << " ns/op" << std::endl;
}

template <typename Key> struct key_traits;

template <> struct key_traits<size_t> {
  static constexpr const char *label = "int";
  static size_t empty_key() noexcept { return 0; }
  static size_t deleted_key() noexcept { return 1; }
  static size_t make_present(size_t idx) noexcept { return idx + 2; }
  static size_t make_missing(size_t idx, size_t count) noexcept {
    return count + idx + 2;
  }
  static size_t value_tag(size_t k) noexcept { return k; }
};

template <> struct key_traits<std::string> {
  static constexpr const char *label = "string";
  static std::string empty_key() { return {}; }
  static std::string deleted_key() { return "\x01"; }
  static std::string format_key(size_t id) {
    // 24-byte string to exceed typical SSO buffers.
    std::string s = "key_00000000000000000000";
    for (size_t pos = s.size(); pos > 4 && id > 0; --pos) {
      s[pos - 1] = static_cast<char>('0' + (id % 10));
      id /= 10;
    }
    return s;
  }
  static std::string make_present(size_t idx) { return format_key(idx + 1); }
  static std::string make_missing(size_t idx, size_t count) {
    return format_key(count + idx + 1);
  }
  static size_t value_tag(const std::string &k) noexcept { return k.size(); }
};

#if defined(HASHMAP_BENCHMARK_HAS_SPARSEHASH)
template <typename Key> struct dense_hash {
  size_t operator()(const Key &k) const noexcept {
    return rigtorp::detail::mix_hash(std::hash<Key>{}(k));
  }
};
#endif

template <typename Key, template <typename> class BaseAlloc>
void run_benchmarks(size_t count, size_t iters, int type,
                    const std::string &workload, const std::string &prefix) {
  struct value {
    Key next;
    char buf[16];
  };

  std::vector<Key> order(count);
  std::vector<Key> erase_order(count);
  std::vector<Key> miss_keys;
  for (size_t i = 0; i < count; ++i) {
    order[i] = key_traits<Key>::make_present(i);
  }
  {
    uint64_t rng = 0;
    for (size_t i = count; i > 1; --i) {
      const size_t j = fast_range(splitmix64(rng), i);
      std::swap(order[i - 1], order[j]);
    }
  }
  erase_order = order;
  {
    uint64_t rng = 1;
    for (size_t i = count; i > 1; --i) {
      const size_t j = fast_range(splitmix64(rng), i);
      std::swap(erase_order[i - 1], erase_order[j]);
    }
  }
  if constexpr (!std::is_same_v<Key, size_t>) {
    miss_keys.resize(count);
    for (size_t i = 0; i < count; ++i) {
      miss_keys[i] = key_traits<Key>::make_missing(i, count);
    }
  }

  auto bench_map = [&](const char *name, auto make_map) {
    if (workload == "all" || workload == "churn") {
      auto m = make_map(count);
      uint64_t rng = 0;
      for (size_t i = 0; i < count; ++i) {
        const size_t idx = fast_range(splitmix64(rng), count);
        if constexpr (std::is_same_v<Key, size_t>) {
          const Key k = key_traits<Key>::make_present(idx);
          m.insert({k, {k, {}}});
        } else {
          m.insert({order[idx], {order[idx], {}}});
        }
      }
      run_timed(prefix, "churn", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const size_t idx = fast_range(splitmix64(rng), count);
          if constexpr (std::is_same_v<Key, size_t>) {
            const Key k = key_traits<Key>::make_present(idx);
            const auto it = m.find(k);
            if (it == m.end()) {
              m.insert({k, {k, {}}});
            } else {
              m.erase(it);
            }
          } else {
            const Key &k = order[idx];
            const auto it = m.find(k);
            if (it == m.end()) {
              m.insert({k, {k, {}}});
            } else {
              m.erase(it);
            }
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
      run_timed(prefix, "hit-throughput", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const size_t idx = fast_range(splitmix64(rng), count);
          if constexpr (std::is_same_v<Key, size_t>) {
            const Key k = key_traits<Key>::make_present(idx);
            sum += key_traits<Key>::value_tag(m.find(k)->second.next);
          } else {
            sum += key_traits<Key>::value_tag(m.find(order[idx])->second.next);
          }
        }
      });
      volatile size_t sink = sum;
      (void)sink;
    }

    if (workload == "all" || workload == "hit-latency") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        const Key &next_k = order[(i + 1) % count];
        m.insert({order[i], {next_k, {}}});
      }
      Key k = order[0];
      run_timed(prefix, "hit-latency", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          k = m.find(k)->second.next;
        }
      });
      volatile size_t sink = key_traits<Key>::value_tag(k);
      (void)sink;
    }

    if (workload == "all" || workload == "miss") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      uint64_t rng = 0;
      size_t hits = 0;
      run_timed(prefix, "miss", name, iters, [&] {
        for (size_t i = 0; i < iters; ++i) {
          const size_t idx = fast_range(splitmix64(rng), count);
          if constexpr (std::is_same_v<Key, size_t>) {
            const Key k = key_traits<Key>::make_missing(idx, count);
            if (m.find(k) != m.end()) {
              ++hits;
            }
          } else {
            if (m.find(miss_keys[idx]) != m.end()) {
              ++hits;
            }
          }
        }
      });
      volatile size_t sink = hits;
      (void)sink;
    }

    if (workload == "all" || workload == "insert") {
      auto m = make_map(0);
      run_timed(prefix, "insert", name, count, [&] {
        for (size_t i = 0; i < count; ++i) {
          m.insert({order[i], {order[i], {}}});
        }
      });
    }

    if (workload == "all" || workload == "insert-reserved") {
      auto m = make_map(count);
      run_timed(prefix, "insert-reserved", name, count, [&] {
        for (size_t i = 0; i < count; ++i) {
          m.insert({order[i], {order[i], {}}});
        }
      });
    }

    if (workload == "all" || workload == "insert-dup") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      run_timed(prefix, "insert-dup", name, count, [&] {
        for (size_t i = 0; i < count; ++i) {
          m.insert({erase_order[i], {erase_order[i], {}}});
        }
      });
    }

    if (workload == "all" || workload == "erase") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      run_timed(prefix, "erase", name, count, [&] {
        for (size_t i = 0; i < count; ++i) {
          m.erase(erase_order[i]);
        }
      });
    }

    if (workload == "all" || workload == "iterate") {
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      const size_t passes =
          std::max<size_t>(1, iters / std::max<size_t>(1, count));
      size_t sum = 0;
      run_timed(prefix, "iterate", name, passes * count, [&] {
        for (size_t p = 0; p < passes; ++p) {
          for (const auto &kv : m) {
            sum += key_traits<Key>::value_tag(kv.second.next);
          }
        }
      });
      volatile size_t sink = sum;
      (void)sink;
    }

    if (workload == "all" || workload == "memory") {
      memory_scope scope;
      auto m = make_map(count);
      for (size_t i = 0; i < count; ++i) {
        m.insert({order[i], {order[i], {}}});
      }
      const double bytes_per_elem =
          static_cast<double>(scope.current()) / static_cast<double>(count);
      const double mib =
          static_cast<double>(scope.current()) / (1024.0 * 1024.0);
      std::cout << prefix << "memory / " << name << ": " << std::fixed
                << std::setprecision(1) << bytes_per_elem << " B/elem (" << mib
                << " MiB)" << std::endl;
    }
  };

  if (type == -1 || type == 1) {
    bench_map("HashMap", [](size_t reserve_count) {
      HashMap<Key, value, std::hash<Key>, std::equal_to<>,
              tracking_allocator<std::pair<Key, value>, BaseAlloc>>
          hm(2, key_traits<Key>::empty_key());
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
      google::dense_hash_map<
          Key, value, dense_hash<Key>, std::equal_to<Key>,
          tracking_allocator<std::pair<const Key, value>, std::allocator>>
          hm(reserve_count);
      hm.set_empty_key(key_traits<Key>::empty_key());
      hm.set_deleted_key(key_traits<Key>::deleted_key());
      return hm;
    });
  }
#endif

#if defined(HASHMAP_BENCHMARK_HAS_ABSL)
  if (type == -1 || type == 3) {
    bench_map("absl::flat_hash_map", [](size_t reserve_count) {
      absl::flat_hash_map<
          Key, value, absl::Hash<Key>, std::equal_to<>,
          tracking_allocator<std::pair<const Key, value>, BaseAlloc>>
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
      std::unordered_map<
          Key, value, std::hash<Key>, std::equal_to<Key>,
          tracking_allocator<std::pair<const Key, value>, std::allocator>>
          hm;
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
          Key, value, boost::hash<Key>, std::equal_to<>,
          tracking_allocator<std::pair<const Key, value>, BaseAlloc>>
          hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }

  if (type == -1 || type == 6) {
    bench_map("boost::unordered_map", [](size_t reserve_count) {
      boost::unordered_map<
          Key, value, boost::hash<Key>, std::equal_to<Key>,
          tracking_allocator<std::pair<const Key, value>, std::allocator>>
          hm;
      if (reserve_count > 0) {
        hm.reserve(reserve_count);
      }
      return hm;
    });
  }
#endif
}

template <template <typename> class BaseAlloc>
void dispatch_key_type(size_t count, size_t iters, int type,
                       const std::string &workload, const std::string &key_type,
                       bool show_prefix) {
  auto prefix_for = [&](const char *klabel) {
    if (!show_prefix && key_type != "all") {
      return std::string{};
    }
    return "[" + std::string(klabel) + " c=" + std::to_string(count) + "] ";
  };
  if (key_type == "all" || key_type == "int") {
    run_benchmarks<size_t, BaseAlloc>(count, iters, type, workload,
                                      prefix_for(key_traits<size_t>::label));
  }
  if (key_type == "all" || key_type == "string") {
    run_benchmarks<std::string, BaseAlloc>(
        count, iters, type, workload,
        prefix_for(key_traits<std::string>::label));
  }
}

} // namespace

int main(int argc, char *argv[]) {
  size_t count = 10000000;
  size_t iters = 100000000;
  int type = -1;
  bool huge_pages = false;
  bool sweep = false;
  std::string key_type = "int";
  std::string workload = "all";

  int opt;
  while ((opt = getopt(argc, argv, "Hsc:i:k:t:w:")) != -1) {
    switch (opt) {
    case 'H':
      huge_pages = true;
      break;
    case 's':
      sweep = true;
      break;
    case 'i':
      iters = std::stoul(optarg);
      break;
    case 'c':
      count = std::stoul(optarg);
      break;
    case 'k':
      key_type = optarg;
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
      (key_type != "int" && key_type != "string" && key_type != "all") ||
      (workload != "all" && workload != "churn" &&
       workload != "hit-throughput" && workload != "hit-latency" &&
       workload != "miss" && workload != "insert" &&
       workload != "insert-reserved" && workload != "insert-dup" &&
       workload != "erase" && workload != "iterate" && workload != "memory")) {
  usage:
    std::cerr << "HashMapBenchmark © 2020 Erik Rigtorp <erik@rigtorp.se>\n"
                 "usage: HashMapBenchmark [-H] [-s] [-c count] [-i iters] [-k "
                 "int|string|all] [-t 1|2|3|4|5|6] [-w "
                 "all|churn|hit-throughput|hit-latency|miss|insert|"
                 "insert-reserved|insert-dup|erase|iterate|memory]\n"
              << std::endl;
    exit(1);
  }

  const std::vector<size_t> counts =
      sweep ? std::vector<size_t>{10000, 100000, 1000000, 10000000}
            : std::vector<size_t>{count};

  for (size_t c : counts) {
    if (huge_pages) {
      dispatch_key_type<huge_page_allocator>(c, iters, type, workload, key_type,
                                             sweep);
    } else {
      dispatch_key_type<std::allocator>(c, iters, type, workload, key_type,
                                        sweep);
    }
  }

  return 0;
}
