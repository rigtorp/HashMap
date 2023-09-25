# HashMap.h

[![C/C++ CI](https://github.com/rigtorp/HashMap/workflows/C/C++%20CI/badge.svg)](https://github.com/rigtorp/HashMap/actions)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/rigtorp/HashMap/master/LICENSE)

A hash table mostly compatible with the C++11 *std::unordered_map*
interface, but with much higher performance for many workloads.

## Implementation

This hash table uses [open addressing][1] with [linear probing][2] and
backshift deletion. Open addressing and linear probing minimizes
memory allocations and achieves high cache efficiency. Backshift deletion
keeps performance high for delete heavy workloads by not clobbering
the hash table with [tombestones][3].

[1]: https://en.wikipedia.org/wiki/Open_addressing "Open addressing"
[2]: https://en.wikipedia.org/wiki/Linear_probing "Linear probing"
[3]: https://en.wikipedia.org/wiki/Lazy_deletion "Lazy deletion"

## Usage

`HashMap` is mostly compatible with the C++11 container interface. The
main differences are:

- A key value to represent the empty key is required.
- `Key` and `T` needs to be default constructible.
- Iterators are invalidated on all modifying operations.
- It's invalid to perform any operations with the empty key.
- Destructors are not called on `erase`.
- Extensions for lookups using related key types.

Member functions:

- `HashMap(size_type bucket_count, key_type empty_key);`

  Construct a `HashMap` with `bucket_count` buckets and `empty_key` as
  the empty key.

The rest of the member functions are implemented as for
[`std::unordered_map`](http://en.cppreference.com/w/cpp/container/unordered_map).

## Example

```cpp
  using namespace rigtorp;

  // Hash for using std::string as lookup key
  struct Hash {
    size_t operator()(int v) { return v * 7; }
    size_t operator()(const std::string &v) { return std::stoi(v) * 7; }
  };

  // Equal comparison for using std::string as lookup key
  struct Equal {
    bool operator()(int lhs, int rhs) { return lhs == rhs; }
    bool operator()(int lhs, const std::string &rhs) {
      return lhs == std::stoi(rhs);
    }
  };

  // Create a HashMap with 16 buckets and 0 as the empty key
  HashMap<int, int, Hash, Equal> hm(16, 0);
  hm.emplace(1, 1);
  hm[2] = 2;

  // Iterate and print key-value pairs
  for (const auto &e : hm) {
    std::cout << e.first << " = " << e.second << "\n";
  }

  // Lookup using std::string
  std::cout << hm.at("1") << "\n";

  // Erase entry
  hm.erase(1);
```

## Benchmark

A benchmark `src/HashMapBenchmark.cpp` is included with the sources. The
benchmark simulates a delete heavy workload where items are repeatedly inserted
and deleted. 

I ran this benchmark on the following configuration:

- AMD Ryzen 9 3900X
- Linux 5.8.4-200.fc32.x86_64
- gcc (GCC) 10.2.1 20200723 (Red Hat 10.2.1-1)
- Isolated a core complex (CCX) using `isolcpus` for running the benchmark

When working set fits in L3 cache (`HashMapBenchmark -c 100000 -i 100000000`):

| Implementation         | mean ns/iter | max ns/iter |
| ---------------------- | -----------: | ----------: |
| HashMap                |           24 |        1082 |
| absl::flat_hash_map    |           24 |        2074 |
| google::dense_hash_map |           49 |      689846 |
| std::unordered_map     |           67 |       10299 |

When working set is larger than L3 cache (`HashMapBenchmark -c 10000000 -i 1000000000`):

| Implementation         | mean ns/iter | max ns/iter |
| ---------------------- | -----------: | ----------: |
| HashMap                |           75 |       19026 |
| absl::flat_hash_map    |          101 |       19848 |
| google::dense_hash_map |          111 |   226083255 |
| std::unordered_map     |          408 |       22422 |


## Cited by

HashMap has been cited by the following papers:
- Koppl, Dominik. “Separate Chaining Meets Compact Hashing.” (2019).
  https://arxiv.org/abs/1905.00163 

## About

This project was created by [Erik Rigtorp](http://rigtorp.se)
<[erik@rigtorp.se](mailto:erik@rigtorp.se)>.
