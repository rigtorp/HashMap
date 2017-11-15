/*
Copyright (c) 2017 Erik Rigtorp <erik@rigtorp.se>

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

/*
HashMap2

A high performance hash map. Uses open addressing with linear
probing.

Advantages:
  - Predictable performance. Doesn't use the allocator unless load factor
    grows beyond 50%. Linear probing ensures cash efficency.
  - Deletes items by rearranging items and marking slots as empty instead of
    marking items as deleted. This is keeps performance high when there
    is a high rate of churn (many paired inserts and deletes) since otherwise
    most slots would be marked deleted and probing would end up scanning
    most of the table.

Disadvantages:
  - Significant performance degradation at high load factors.
  - Maximum load factor hard coded to 50%, memory inefficient.
  - Memory is not reclaimed on erase.
 */

#pragma once

#include "emmintrin.h"
#include "immintrin.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rigtorp {

namespace detail {

struct Hash {
  template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  uint64_t operator()(T v) const noexcept {
    auto h = static_cast<uint64_t>(v);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
  }
};

template <typename T> class BitsetIndexIterator {
public:
  BitsetIndexIterator() noexcept = default;

  explicit BitsetIndexIterator(T bitset) noexcept : bitset_(bitset) {}

  int operator*() const noexcept { return ctz(bitset_); }

  BitsetIndexIterator &operator++() noexcept {
    bitset_ &= ~(static_cast<T>(1) << operator*());
    return *this;
  }

  bool operator==(const BitsetIndexIterator &other) const noexcept {
    return other.bitset_ == bitset_;
  }

  bool operator!=(const BitsetIndexIterator &other) const noexcept {
    return !(other == *this);
  }

private:
  static constexpr int ctz(uint32_t x) {
    static_assert(std::is_same<unsigned int, T>::value);
    return __builtin_ctz(x);
  }

  static constexpr int ctz(uint64_t x) {
    static_assert(std::is_same<unsigned long, T>::value);
    return __builtin_ctzl(x);
  }

private:
  T bitset_ = 0;
};

template <typename T> class Bitset {
public:
  Bitset() noexcept = default;

  explicit Bitset(T bitset) noexcept : bitset_(bitset) {}

  BitsetIndexIterator<T> begin() const noexcept {
    return BitsetIndexIterator<T>(bitset_);
  }

  BitsetIndexIterator<T> end() const noexcept { return {}; }

  explicit operator bool() const noexcept { return bitset_ != 0; }

private:
  const T bitset_ = 0;
};

class Group {
public:
  explicit Group(const char *group) : group_(group) {}

  static constexpr int size() {
#ifdef __AVX2__
    return 32;
#elif __AVX__
    return 16;
#else
    return 1;
#endif
  }

  Bitset<uint32_t> matching(char hash) const noexcept {
#ifdef __AVX2__
    const auto mask = _mm256_set1_epi8(hash);
    const auto ctrl =
        _mm256_loadu_si256(reinterpret_cast<const __m256i_u *>(group_));
    const auto matching = _mm256_movemask_epi8(_mm256_cmpeq_epi8(mask, ctrl));
#elif __AVX__
    const auto mask = _mm_set1_epi8(hash);
    const auto ctrl =
        _mm_loadu_si128(reinterpret_cast<const __m128i_u *>(group_));
    const auto matching = _mm_movemask_epi8(_mm_cmpeq_epi8(mask, ctrl));
#else
    const bool matching = group_[0] == hash;
#endif
    return Bitset<uint32_t>(matching);
  }

  Bitset<uint32_t> empty_buckets() const noexcept { return matching(-128); }

  Bitset<uint32_t> available_buckets() const noexcept {
#ifdef __AVX2__
    const auto ctrl =
        _mm256_loadu_si256(reinterpret_cast<const __m256i_u *>(group_));
    const auto available = _mm256_movemask_epi8(ctrl);
#elif __AVX__
    const auto ctrl =
        _mm_loadu_si128(reinterpret_cast<const __m128i_u *>(group_));
    const auto available = _mm_movemask_epi8(ctrl);
#else
    const bool available = group_[0] & -128;
#endif
    return Bitset<uint32_t>(available);
  }

private:
  const char *const group_;
};
}

template <typename Key, typename T, typename Hash = detail::Hash,
          typename KeyEqual = std::equal_to<void>>
class HashMap2 {
public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<const Key, T>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using reference = value_type &;
  using const_reference = const value_type &;

  template <typename ContT, typename IterVal> struct hm_iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = IterVal;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::forward_iterator_tag;

    bool operator==(const hm_iterator &other) const {
      return other.hm_ == hm_ && other.idx_ == idx_;
    }
    bool operator!=(const hm_iterator &other) const {
      return !(other == *this);
    }

    hm_iterator &operator++() {
      ++idx_;
      advance_past_empty();
      return *this;
    }

    reference operator*() const { return hm_->buckets_[idx_]; }
    pointer operator->() const { return &hm_->buckets_[idx_]; }

  private:
    explicit hm_iterator(ContT *hm) : hm_(hm) { advance_past_empty(); }
    explicit hm_iterator(ContT *hm, size_type idx) : hm_(hm), idx_(idx) {}
    template <typename OtherContT, typename OtherIterVal>
    hm_iterator(const hm_iterator<OtherContT, OtherIterVal> &other)
        : hm_(other.hm_), idx_(other.idx_) {}

    void advance_past_empty() {
      while (idx_ < hm_->num_buckets_ && hm_->ctrl_[idx_] & -128) {
        ++idx_;
      }
    }

    ContT *hm_ = nullptr;
    typename ContT::size_type idx_ = 0;
    friend ContT;
  };

  using iterator = hm_iterator<HashMap2, value_type>;
  using const_iterator = hm_iterator<const HashMap2, const value_type>;

public:
  HashMap2() : HashMap2(0){};

  explicit HashMap2(size_type bucket_count) {
    bucket_count = std::max<size_t>(bucket_count, 256);
    size_t pow2 = 1;
    while (pow2 < bucket_count) {
      pow2 <<= 1;
    }
    bucket_count = pow2;
    void *ptr = operator new(sizeof(value_type) * bucket_count + bucket_count);
    buckets_ = static_cast<value_type *>(ptr);
    ctrl_ = static_cast<char *>(ptr) + sizeof(value_type) * bucket_count;
    std::fill(ctrl_, ctrl_ + bucket_count, -128);
    num_buckets_ = bucket_count;
  }

  template <typename InputIt>
  HashMap2(InputIt first, InputIt last)
      : HashMap2(first, last, std::distance(first, last)) {
    // TODO size according to load factor
  }

  template <typename InputIt>
  HashMap2(InputIt first, InputIt last, size_type bucket_count)
      : HashMap2(bucket_count) {
    insert(first, last);
  }

  HashMap2(const HashMap2 &other)
      : HashMap2(other.begin(), other.end(), other.size()) {
    // TODO optimize for the case when value_type is trivially_copyable
    // TODO size according to load factor
  }

  HashMap2(HashMap2 &&other) noexcept { swap(other); }

  ~HashMap2() {
    if (!std::is_trivially_destructible<value_type>::value) {
      erase(begin(), end());
    }
    operator delete(buckets_);
  }

  HashMap2 &operator=(const HashMap2 &other) {
    if (&other != this) {
      HashMap2 copy(other);
      swap(copy);
    }
    return *this;
  }

  HashMap2 &operator=(HashMap2 &&other) noexcept {
    swap(other);
    return *this;
  }

  // Iterators
  iterator begin() { return iterator(this); }

  const_iterator begin() const { return const_iterator(this); }

  const_iterator cbegin() const { return const_iterator(this); }

  iterator end() { return iterator(this, num_buckets_); }

  const_iterator end() const { return const_iterator(this, num_buckets_); }

  const_iterator cend() const { return const_iterator(this, num_buckets_); }

  // Capacity
  bool empty() const noexcept { return size() == 0; }

  size_type size() const noexcept { return num_entries_; }

  size_type max_size() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  // Modifiers
  void clear() {
    HashMap2 other;
    swap(other);
  }

  std::pair<iterator, bool> insert(const value_type &value) {
    return emplace_impl(value.first, value.second);
  }

  std::pair<iterator, bool> insert(value_type &&value) {
    return emplace_impl(value.first, std::move(value.second));
  }

  template <typename InputIt> void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type &k, Args &&... args) {
    return emplace_impl(k, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type &&k, Args &&... args) {
    return emplace_impl(std::move(k), std::forward<Args>(args)...);
  }

  void erase(iterator it) { erase_impl(it); }

  void erase(iterator first, iterator last) {
    for (; first != last; ++first) {
      erase(first);
    }
  }

  size_type erase(const key_type &key) { return erase_impl(key); }

  template <typename K> size_type erase(const K &x) { return erase_impl(x); }

  void swap(HashMap2 &other) {
    std::swap(buckets_, other.buckets_);
    std::swap(ctrl_, other.ctrl_);
    std::swap(num_buckets_, other.num_buckets_);
    std::swap(num_entries_, other.num_entries_);
    std::swap(num_tombestones_, other.num_tombestones_);
  }

  // Lookup
  mapped_type &at(const key_type &key) { return at_impl(key); }

  template <typename K> mapped_type &at(const K &x) { return at_impl(x); }

  const mapped_type &at(const key_type &key) const { return at_impl(key); }

  template <typename K> const mapped_type &at(const K &x) const {
    return at_impl(x);
  }

  mapped_type &operator[](const key_type &key) {
    return emplace_impl(key, mapped_type{}).first->second;
  }

  size_type count(const key_type &key) const noexcept {
    return count_impl(key);
  }

  template <typename K> size_type count(const K &x) const noexcept {
    return count_impl(x);
  }

  iterator find(const key_type &key) noexcept { return find_impl(key); }

  template <typename K> iterator find(const K &x) noexcept {
    return find_impl(x);
  }

  const_iterator find(const key_type &key) const noexcept {
    return find_impl(key);
  }

  template <typename K> const_iterator find(const K &x) const noexcept {
    return find_impl(x);
  }

  // Bucket interface
  size_type bucket_count() const noexcept { return num_buckets_; }

  size_type max_bucket_count() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  // Hash policy
  float load_factor() const noexcept {
    return static_cast<float>(size() + num_tombestones_) / bucket_count();
  }

  static const size_t k_max_load_factor = 28;

  float max_load_factor() const noexcept {
    return static_cast<float>(k_max_load_factor) / 32;
  }

  void rehash(size_type count) {
    // TODO rehash inplace if bucket count did not change
    count = std::max(count, (size() * k_max_load_factor) / 32 + 1);
    HashMap2 other(begin(), end(), count);
    swap(other);
  }

  void reserve(size_type count) {
    if ((count + num_tombestones_) * 32 > bucket_count() * k_max_load_factor) {
      rehash((count * 32) / 24 + 1);
    }
    if (count * 32 > bucket_count() * 24) {
      rehash((count * 32) / 24 + 1);
    }
  }

  // Observers
  hasher hash_function() const { return hasher(); }

  key_equal key_eq() const { return key_equal(); }

private:
  using Group = detail::Group;

  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(const K &key, Args &&... args) {
    const auto hash = hasher()(key);
    const auto it = find_impl(key, hash);
    if (it != end()) {
      return {it, false};
    }
    reserve(num_entries_ + 1);
    const auto h1 = hash >> 7;
    const auto h2 = hash & 0x7f;
    for (auto group_idx = h1 & (num_buckets_ / Group::size() - 1);;
         group_idx = (group_idx + 1) & (num_buckets_ / Group::size() - 1)) {
      const auto group = Group(&ctrl_[group_idx * Group::size()]);
      for (const int idx : group.available_buckets()) {
        const auto bucket_idx = group_idx * group.size() + idx;
        if (ctrl_[bucket_idx] == -1) {
          num_tombestones_--;
        }
        ctrl_[bucket_idx] = h2;
        new (&buckets_[bucket_idx])
            value_type(key, std::forward<Args>(args)...);
        num_entries_++;
        return {iterator(this, bucket_idx), true};
      }
    }
  }

  void erase_impl(iterator it) noexcept(
      std::is_nothrow_destructible<value_type>::value) {
    assert(it.hm_ == this);
    assert(it.idx_ < num_buckets_);
    size_t bucket_idx = it.idx_;
    assert(ctrl_[bucket_idx] ==
           static_cast<char>(hasher()(buckets_[bucket_idx].first) & 0x7f));
    const auto group = Group(&ctrl_[bucket_idx & (Group::size() - 1)]);
    buckets_[bucket_idx].~value_type();
    if (group.empty_buckets()) {
      ctrl_[bucket_idx] = -128;
    } else {
      ctrl_[bucket_idx] = -1;
      num_tombestones_++;
    }
    num_entries_--;
  }

  template <typename K> size_type erase_impl(const K &key) {
    const auto it = find_impl(key);
    if (it != end()) {
      erase_impl(it);
      return 1;
    }
    return 0;
  }

  template <typename K> mapped_type &at_impl(const K &key) {
    const auto it = find_impl(key);
    if (it != end()) {
      return it->second;
    }
    throw std::out_of_range("HashMap2::at");
  }

  template <typename K> const mapped_type &at_impl(const K &key) const {
    return const_cast<HashMap2 *>(this)->at_impl(key);
  }

  template <typename K> size_t count_impl(const K &key) const noexcept {
    return find_impl(key) == end() ? 0 : 1;
  }

  template <typename K> iterator find_impl(const K &key) {
    return find_impl(key, hasher()(key));
  }

  template <typename K> const_iterator find_impl(const K &key) const {
    return const_cast<HashMap2 *>(this)->find_impl(key);
  }

  template <typename K> iterator find_impl(const K &key, size_type hash) {
    const auto h1 = hash >> 7;
    const auto h2 = hash & 0x7f;
    for (auto group_idx = h1 & (num_buckets_ / Group::size() - 1);;
         group_idx = (group_idx + 1) & (num_buckets_ / Group::size() - 1)) {
      const auto group = Group(&ctrl_[group_idx * Group::size()]);
      for (const int idx : group.matching(h2)) {
        const auto bucket_idx = group_idx * Group::size() + idx;
        if (key_equal()(buckets_[bucket_idx].first, key)) {
          return iterator(this, bucket_idx);
        }
      }
      if (group.empty_buckets()) {
        return end();
      }
    }
  }

private:
  value_type *buckets_ = nullptr;
  char *ctrl_ = nullptr;
  size_t num_buckets_ = 0;
  size_t num_entries_ = 0;
  size_t num_tombestones_ = 0;
};
}
