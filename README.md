# HashMap.h

[![Build Status](https://travis-ci.org/rigtorp/HashMap.svg?branch=master)](https://travis-ci.org/rigtorp/HashMap)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/rigtorp/HashMap/master/LICENSE)

A hash table mostly compatible with the C++11 *std::unordered_map*
interface, but with much higher performance for many workloads.

## Implementation

This hash table uses open addressing with linear probing and backshift
deletion. Open addressing and linear probing minimizes memory
allocations and achives high cache effiency. Backshift deletion keeps
performance high for delete heavy workloads by not clobbering the hash
table with tombestones.

Please note that this hash table currently only works with POD-types,
destructors are not called on *erase()*. It's not too hard to make it
work with complex types.

## Benchmark

The benchmark first inserts 1M random entries in the table and then
removes the last inserted item and inserts a new random entry 1
billion times. This is benchmark is designed to simulate a delete
heavy workload.

| Implementation         | ns/iter |
| ---------------------- | -------:|
| HashMap                |      77 |
| google::dense_hash_map |     122 |
| std::unordered_map     |     220 |

## Example

```cpp
// Create a HashMap with 16 buckets and 0 as the empty key
HashMap<int, int> hm(16, 0);
hm.emplace(1, 1);
hm[2] = 2;

// Iterate and print key-value pairs
for (const auto &e : hm) {
  std::cout << e.first << " = " << e.second << "\n";
}

// Erase entry
hm.erase(1);
```

## About

This project was created by [Erik Rigtorp](http://rigtorp.se)
<[erik@rigtorp.se](mailto:erik@rigtorp.se)>.
