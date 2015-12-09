# HashMap.h

A hash table mostly compatible with the C++11 *std::unordered_map*
interface, but with much higher performance.

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

```
HashMap:                77 ns/iter
google::dense_hash_map: 122 ns/iter
std::unordered_map:     220 ns/iter
```
