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

/*
struct Hash {
template <typename T>
std::enable_if_t<std::is_integral<T>::value, uint64_t> operator()(T v) const
  noexcept {
uint64_t h = static_cast<uint64_t>(v);
h ^= h >> 33;
h *= 0xff51afd7ed558ccd;
h ^= h >> 33;
h *= 0xc4ceb9fe1a85ec53;
h ^= h >> 33;
return h;
}
};
*/

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<void>>
class HashMap2 {
public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<Key, T>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using reference = value_type &;
  using const_reference = const value_type &;
  using buckets = std::vector<value_type>;

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
      while (idx_ < hm_->buckets_.size() && hm_->ctrl_[idx_] & -128) {
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
  HashMap2() : HashMap2(0) {}

  explicit HashMap2(size_type bucket_count) {
    bucket_count = std::max<size_t>(bucket_count, 32);
    size_t pow2 = 1;
    while (pow2 < bucket_count) {
      pow2 <<= 1;
    }
    buckets_.resize(pow2);
    ctrl_.resize(pow2, -128);
  }

  HashMap2(const HashMap2 &other, size_type bucket_count)
      : HashMap2(bucket_count) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(*it);
    }
  }

  // Iterators
  iterator begin() { return iterator(this); }

  const_iterator begin() const { return const_iterator(this); }

  const_iterator cbegin() const { return const_iterator(this); }

  iterator end() { return iterator(this, buckets_.size()); }

  const_iterator end() const { return const_iterator(this, buckets_.size()); }

  const_iterator cend() const { return const_iterator(this, buckets_.size()); }

  // Capacity
  bool empty() const noexcept { return size() == 0; }

  size_type size() const noexcept { return size_; }

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

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  void erase(iterator it) { erase_impl(it); }

  size_type erase(const key_type &key) { return erase_impl(key); }

  template <typename K> size_type erase(const K &x) { return erase_impl(x); }

  void swap(HashMap2 &other) {
    std::swap(buckets_, other.buckets_);
    std::swap(ctrl_, other.ctrl_);
    std::swap(size_, other.size_);
  }

  // Lookup
  mapped_type &at(const key_type &key) { return at_impl(key); }

  template <typename K> mapped_type &at(const K &x) { return at_impl(x); }

  const mapped_type &at(const key_type &key) const { return at_impl(key); }

  template <typename K> const mapped_type &at(const K &x) const {
    return at_impl(x);
  }

  mapped_type &operator[](const key_type &key) {
    return emplace_impl(key).first->second;
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
  size_type bucket_count() const noexcept { return buckets_.size(); }

  size_type max_bucket_count() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  // Hash policy
  float load_factor() const noexcept {
    return static_cast<float>(size() + tombestones_) / bucket_count();
  }

  float max_load_factor() const noexcept { return static_cast<float>(7) / 8; }

  // void max_load_factor(float ml) {}

  void rehash(size_type count) {
    count = std::max(count, (size() * 7) / 8);
    HashMap2 other(*this, count);
    swap(other);
  }

  void reserve(size_type count) {
    if ((count + tombestones_) * 8 > buckets_.size() * 7) {
      rehash(count);
    }
  }

  // Observers
  hasher hash_function() const { return hasher(); }

  key_equal key_eq() const { return key_equal(); }

private:
  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(const K &key, Args &&... args) {
    reserve(size_ + 1);
    const auto hash = hasher()(key);
#ifdef __AVX2__
#warning "using avx2"
    auto group = hash >> 7 & (buckets_.size() / 32 - 1);
    for (;;) {
      const auto mask = _mm256_set1_epi8(hash & 0x7F);
      const auto ctrl =
          _mm256_loadu_si256((const __m256i_u *)&ctrl_[group * 32]);
      auto bitset = _mm256_movemask_epi8(_mm256_cmpeq_epi8(mask, ctrl));

      while (bitset) {
        int i = __builtin_ctz(bitset);
        if (key_equal()(buckets_[group * 32 + i].first, key)) {
          return {iterator(this, group * 32 + i), false};
        }
        bitset &= ~(1 << i);
      }

      auto empty = _mm256_movemask_epi8(ctrl);
      if (empty) {
        int i = __builtin_ctz(empty);
        const auto idx = group * 32 + i;
        ctrl_[idx] = hash & 0x7F;
        buckets_[idx].second = mapped_type(std::forward<Args>(args)...);
        buckets_[idx].first = key;
        size_++;
        return {iterator(this, idx), true};
      }

      group = (group + 1) & (buckets_.size() / 32 - 1);
    }
#elif __AVX__
    auto group = hash >> 7 & (buckets_.size() / 16 - 1);
    for (;;) {
      const auto mask = _mm_set1_epi8(hash & 0x7F);
      const auto ctrl = _mm_loadu_si128((const __m128i_u *)&ctrl_[group * 16]);
      auto bitset = _mm_movemask_epi8(_mm_cmpeq_epi8(mask, ctrl));

      while (bitset) {
        int i = __builtin_ctz(bitset);
        if (key_equal()(buckets_[group * 16 + i].first, key)) {
          return {iterator(this, group * 16 + i), false};
        }
        bitset &= ~(1 << i);
      }

      auto empty = _mm_movemask_epi8(ctrl);
      if (empty) {
        int i = __builtin_ctz(empty);
        const auto idx = group * 16 + i;
        ctrl_[idx] = hash & 0x7F;
        buckets_[idx].second = mapped_type(std::forward<Args>(args)...);
        buckets_[idx].first = key;
        size_++;
        return {iterator(this, idx), true};
      }

      group = (group + 1) & (buckets_.size() / 16 - 1);
    }
#else
#error "cannot vecotirze"
#endif
  }

  void erase_impl(iterator it) {
    size_t bucket = it.idx_;
#ifdef __AVX2__
#warning "using avx2"
    const auto group = bucket / 32;
    const auto mask2 = _mm256_set1_epi8(-128);
    const auto ctrl = _mm256_loadu_si256((const __m256i_u *)&ctrl_[group * 32]);
    const auto empty = _mm256_movemask_epi8(_mm256_cmpeq_epi8(mask2, ctrl));
#elif __AVX__
    const auto group = bucket / 16;
    const auto mask2 = _mm_set1_epi8(-128);
    const auto ctrl = _mm_loadu_si128((const __m128i_u *)&ctrl_[group * 16]);
    const auto empty = _mm_movemask_epi8(_mm_cmpeq_epi8(mask2, ctrl));
#else
#error "cannot vectorize"
#endif
    if (empty) {
      ctrl_[bucket] = -128;
    } else {
      ctrl_[bucket] = -1;
      tombestones_++;
    }
    size_--;
  }

  template <typename K> size_type erase_impl(const K &key) {
    auto it = find_impl(key);
    if (it != end()) {
      erase_impl(it);
      return 1;
    }
    return 0;
  }

  template <typename K> mapped_type &at_impl(const K &key) {
    iterator it = find_impl(key);
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

  template <typename K>
  iterator find_impl(const K &key) noexcept(noexcept(hasher()(key))) {
    const auto hash = hasher()(key);
#ifdef __AVX2__
#warning "using avx2"
    auto group = hash >> 7 & (buckets_.size() / 32 - 1);
    for (;;) {
      const auto mask = _mm256_set1_epi8(hash & 0x7F);
      const auto ctrl =
          _mm256_loadu_si256((const __m256i_u *)&ctrl_[group * 32]);
      auto bitset = _mm256_movemask_epi8(_mm256_cmpeq_epi8(mask, ctrl));

      while (bitset) {
        int i = __builtin_ctz(bitset);
        if (key_equal()(buckets_[group * 32 + i].first, key)) {
          return iterator(this, group * 32 + i);
        }
        bitset &= ~(1 << i);
      }

      const auto mask2 = _mm256_set1_epi8(-128);
      const auto empty = _mm256_movemask_epi8(_mm256_cmpeq_epi8(mask2, ctrl));
      if (empty) {
        return end();
      }

      group = (group + 1) & (buckets_.size() / 32 - 1);
    }
#else
#ifdef __AVX__
#warning "using avx"
    auto group = group_idx(hash);
    for (;;) {
      const auto mask = _mm_set1_epi8(hash & 0x7F);
      const auto ctrl = _mm_loadu_si128((const __m128i_u *)&ctrl_[group * 16]);
      auto bitset = _mm_movemask_epi8(_mm_cmpeq_epi8(mask, ctrl));

      while (bitset) {
        int i = __builtin_ctz(bitset);
        if (key_equal()(buckets_[group * 16 + i].first, key)) {
          return iterator(this, group * 16 + i);
        }
        bitset &= ~(1 << i);
      }

      const auto mask2 = _mm_set1_epi8(-128);
      const auto empty = _mm_movemask_epi8(_mm_cmpeq_epi8(mask2, ctrl));
      if (empty) {
        return end();
      }

      group = (group + 1) & (buckets_.size() / 16 - 1);
    }
#else
#error "cannot vectorize"
#endif
#endif
  }

  template <typename K> const_iterator find_impl(const K &key) const {
    return const_cast<HashMap2 *>(this)->find_impl(key);
  }

  template <typename K> size_t key_to_idx(const K &key) const {
    const size_t mask = buckets_.size() - 1;
    return hasher()(key) & mask;
  }

  size_t group_idx(const size_t hash) {
    return hash >> 7 & (buckets_.size() / 16 - 1);
  }

  size_t probe_next(size_t idx) const {
    const size_t mask = buckets_.size() - 1;
    return (idx + 1) & mask;
  }

  size_t diff(size_t a, size_t b) const {
    const size_t mask = buckets_.size() - 1;
    return (buckets_.size() + (a - b)) & mask;
  }

private:
  buckets buckets_;
  std::vector<int8_t> ctrl_;
  size_t size_ = 0;
  size_t tombestones_ = 0;
};
}
