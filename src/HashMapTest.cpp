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

#include <algorithm>
#include <array>
#include <string>

#include <rigtorp/HashMap.h>

using namespace rigtorp;

static bool ok = true;

#define EXPECT(expr)                                                           \
  ([](bool res) {                                                              \
    if (!res) {                                                                \
      fprintf(stdout, "FAILED %s:%i: %s\n", __FILE__, __LINE__, #expr);        \
    }                                                                          \
    ok = ok && res;                                                            \
  }(static_cast<bool>(expr)))
#define THROWS(expr)                                                           \
  ([&]() {                                                                     \
    try {                                                                      \
      expr;                                                                    \
    } catch (...) {                                                            \
      return true;                                                             \
    }                                                                          \
    return false;                                                              \
  }())

struct Hash {
  size_t operator()(int v) { return v * 7; }
  size_t operator()(const std::string &v) { return std::stoi(v) * 7; }
};

struct Equal {
  bool operator()(int lhs, int rhs) { return lhs == rhs; }
  bool operator()(int lhs, const std::string &rhs) {
    return lhs == std::stoi(rhs);
  }
};

int main(int argc, char *argv[]) {
  (void)argc, (void)argv;

  // Correct types
  {
    HashMap<int, int, Hash, Equal> hm(16, 0);
    const auto &chm = hm;

    // Iterators
    static_assert(
        std::is_same<decltype(hm.begin()), decltype(hm)::iterator>::value, "");
    static_assert(std::is_same<decltype(chm.begin()),
                               decltype(hm)::const_iterator>::value,
                  "");
    static_assert(std::is_same<decltype(hm.cbegin()),
                               decltype(hm)::const_iterator>::value,
                  "");
    static_assert(
        std::is_same<decltype(hm.end()), decltype(hm)::iterator>::value, "");
    static_assert(
        std::is_same<decltype(chm.end()), decltype(hm)::const_iterator>::value,
        "");
    static_assert(
        std::is_same<decltype(hm.cend()), decltype(hm)::const_iterator>::value,
        "");

    // Capacity
    static_assert(std::is_same<decltype(hm.empty()), bool>::value, "");
    static_assert(std::is_same<decltype(chm.empty()), bool>::value, "");
    static_assert(std::is_same<decltype(hm.size()), size_t>::value, "");
    static_assert(std::is_same<decltype(chm.size()), size_t>::value, "");
    static_assert(std::is_same<decltype(hm.max_size()), size_t>::value, "");
    static_assert(std::is_same<decltype(chm.max_size()), size_t>::value, "");

    // Modifiers
    const auto &p = std::make_pair(1, 1);
    static_assert(std::is_same<decltype(hm.insert(p)),
                               std::pair<decltype(hm)::iterator, bool>>::value,
                  "");
    static_assert(std::is_same<decltype(hm.insert(std::make_pair(1, 1))),
                               std::pair<decltype(hm)::iterator, bool>>::value,
                  "");
    static_assert(std::is_same<decltype(hm.emplace(1, 1)),
                               std::pair<decltype(hm)::iterator, bool>>::value,
                  "");
    static_assert(std::is_same<decltype(hm.erase(hm.begin())), void>::value,
                  "");
    static_assert(std::is_same<decltype(hm.erase(1)), size_t>::value, "");
    static_assert(std::is_same<decltype(hm.erase("1")), size_t>::value, "");

    // Lookup

    // at()
    static_assert(std::is_same<decltype(hm.at(1)), int &>::value, "");
    static_assert(std::is_same<decltype(hm.at("1")), int &>::value, "");
    static_assert(std::is_same<decltype(chm.at(1)), const int &>::value, "");
    static_assert(std::is_same<decltype(chm.at("1")), const int &>::value, "");

    // operator[]()
    static_assert(std::is_same<decltype(hm[1]), int &>::value, "");

    // count()
    static_assert(std::is_same<decltype(hm.count(1)), size_t>::value, "");
    static_assert(std::is_same<decltype(hm.count("1")), size_t>::value, "");
    static_assert(std::is_same<decltype(chm.count(1)), size_t>::value, "");
    static_assert(std::is_same<decltype(chm.count("1")), size_t>::value, "");

    // find()
    static_assert(
        std::is_same<decltype(hm.find(1)), decltype(hm)::iterator>::value, "");
    static_assert(
        std::is_same<decltype(hm.find("1")), decltype(hm)::iterator>::value,
        "");
    static_assert(std::is_same<decltype(chm.find(1)),
                               decltype(hm)::const_iterator>::value,
                  "");
    static_assert(std::is_same<decltype(chm.find("1")),
                               decltype(hm)::const_iterator>::value,
                  "");

    // Bucket interface

    // bucket_count()
    static_assert(std::is_same<decltype(hm.bucket_count()), size_t>::value, "");
    static_assert(std::is_same<decltype(chm.bucket_count()), size_t>::value,
                  "");

    // max_bucket_count()
    static_assert(std::is_same<decltype(hm.max_bucket_count()), size_t>::value,
                  "");
    static_assert(std::is_same<decltype(chm.max_bucket_count()), size_t>::value,
                  "");

    // Hash policy

    // rehash()
    static_assert(std::is_same<decltype(hm.rehash(1)), void>::value, "");

    // reserve()
    static_assert(std::is_same<decltype(hm.reserve(1)), void>::value, "");

    // Observers

    // hash_function()
    static_assert(
        std::is_same<decltype(hm.hash_function()), decltype(hm)::hasher>::value,
        "");
    static_assert(std::is_same<decltype(chm.hash_function()),
                               decltype(hm)::hasher>::value,
                  "");

    // key_eq()
    static_assert(
        std::is_same<decltype(hm.key_eq()), decltype(hm)::key_equal>::value,
        "");
    static_assert(
        std::is_same<decltype(chm.key_eq()), decltype(hm)::key_equal>::value,
        "");
  }

  // Constructors
  {
    // HashMap(const HashMap&)
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    HashMap<int, int> hm2(hm);
    EXPECT(!hm2.empty());
    EXPECT(hm2.size() == 1);
    EXPECT(hm2[1] == 1);
  }

  {
    // HashMap(HashMap&&)
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    HashMap<int, int> hm2(std::move(hm));
    EXPECT(!hm2.empty());
    EXPECT(hm2.size() == 1);
    EXPECT(hm2[1] == 1);
  }

  {
    // operator=(const HashMap&)
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    HashMap<int, int> hm2(16, 0);
    hm2.operator=(hm);
    EXPECT(!hm2.empty());
    EXPECT(hm2.size() == 1);
    EXPECT(hm2[1] == 1);
  }

  {
    // operator=(HashMap&&)
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    HashMap<int, int> hm2(16, 0);
    hm2.operator=(std::move(hm));
    EXPECT(!hm2.empty());
    EXPECT(hm2.size() == 1);
    EXPECT(hm2[1] == 1);
  }

  // Iterators
  {
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;

    EXPECT(hm.begin() == hm.end());
    EXPECT(chm.begin() == chm.end());
    EXPECT(hm.cbegin() == hm.cend());
    EXPECT(hm.cbegin() == chm.begin());
    EXPECT(hm.cend() == chm.end());

    EXPECT(!(hm.begin() != hm.end()));
    EXPECT(!(chm.begin() != chm.end()));
    EXPECT(!(hm.cbegin() != hm.cend()));
    EXPECT(!(hm.cbegin() != chm.begin()));
    EXPECT(!(hm.cend() != chm.end()));

    const auto cit = hm.begin();
    EXPECT(cit == hm.end());
    EXPECT(!(cit != hm.end()));

    for (int i = 1; i < 100; ++i) {
      hm[i] = i;
    }

    std::array<bool, 100> visited = {};
    for (auto it = hm.begin(); it != hm.end(); ++it) {
      visited[it->first] = true;
    }

    for (int i = 1; i < 100; ++i) {
      EXPECT(visited[i]);
    }

    // Test for iterator traits
    EXPECT(std::all_of(hm.begin(), hm.end(),
                       [](const auto &item) { return item.second > 0; }));
  }

  // Capacity
  {
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    EXPECT(chm.empty());
    EXPECT(chm.size() == 0);
    EXPECT(chm.max_size() > 0);
    hm[1] = 1;
    EXPECT(!chm.empty());
    EXPECT(chm.size() == 1);
  }

  // Modifiers
  {
    // clear()
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    hm.clear();
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
    EXPECT(hm.begin() == hm.end());
    EXPECT(hm.cbegin() == hm.cend());
  }

  {
    // insert()
    HashMap<int, int> hm(16, 0);
    auto res = hm.insert({1, 1}); // xvalue
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    EXPECT(hm.begin() != hm.end());
    EXPECT(hm.cbegin() != hm.cend());
    EXPECT(res.first != hm.end());
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
    EXPECT(res.second);
    const auto v = std::make_pair(1, 2);
    auto res2 = hm.insert(v); // rvalue
    EXPECT(hm.size() == 1);
    EXPECT(res2.first == res.first);
    EXPECT(res2.first->first == 1);
    EXPECT(res2.first->second == 1);
    EXPECT(!res2.second);
  }

  {
    // emplace()
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    EXPECT(hm.begin() != hm.end());
    EXPECT(hm.cbegin() != hm.cend());
    EXPECT(res.first != hm.end());
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
    EXPECT(res.second);
    auto res2 = hm.emplace(1, 2);
    EXPECT(hm.size() == 1);
    EXPECT(res2.first == res.first);
    EXPECT(res2.first->first == 1);
    EXPECT(res2.first->second == 1);
    EXPECT(!res2.second);
  }

  {
    // erase(iterator)
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    hm.erase(res.first);
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
    EXPECT(hm.begin() == hm.end());
    EXPECT(hm.cbegin() == hm.cend());
  }

  {
    // erase(const key_type&)
    HashMap<int, int> hm(16, 0);
    EXPECT(hm.erase(1) == 0);
    hm[1] = 1;
    EXPECT(hm.erase(1) == 1);
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
    EXPECT(hm.begin() == hm.end());
    EXPECT(hm.cbegin() == hm.cend());
  }

  {
    // template <class K> erase(const K&)
    HashMap<int, int, Hash, Equal> hm(16, 0);
    EXPECT(hm.erase("1") == 0);
    hm[1] = 1;
    EXPECT(hm.erase("1") == 1);
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
    EXPECT(hm.begin() == hm.end());
    EXPECT(hm.cbegin() == hm.cend());
  }

  {
    // swap()
    HashMap<int, int> hm1(16, 0), hm2(16, 0);
    hm1[1] = 1;
    hm2.swap(hm1);
    EXPECT(hm1.empty());
    EXPECT(hm1.size() == 0);
    EXPECT(hm2.size() == 1);
    EXPECT(hm2[1] == 1);
    std::swap(hm1, hm2);
    EXPECT(hm1.size() == 1);
    EXPECT(hm1[1] == 1);
    EXPECT(hm2.empty());
    EXPECT(hm2.size() == 0);
  }

  // Lookup
  {
    // at(const key_type&)
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    EXPECT(hm.at(1) == 1);
    EXPECT(chm.at(1) == 1);
    hm.at(1) = 2;
    EXPECT(hm.at(1) == 2);
    EXPECT(chm.at(1) == 2);
    EXPECT(THROWS(hm.at(2)));
    EXPECT(THROWS(chm.at(2)));
  }

  {
    // template <class K> at(const K&)
    HashMap<int, int, Hash, Equal> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    EXPECT(hm.at("1") == 1);
    EXPECT(chm.at("1") == 1);
    hm.at("1") = 2;
    EXPECT(hm.at("1") == 2);
    EXPECT(chm.at("1") == 2);
    EXPECT(THROWS(hm.at("2")));
    EXPECT(THROWS(chm.at("2")));
  }

  {
    // operator[](const key_type&)
    HashMap<int, int> hm(16, 0);
    hm[1] = 1;
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    EXPECT(hm.begin() != hm.end());
    EXPECT(hm.cbegin() != hm.cend());
    EXPECT(hm[1] == 1);
  }

  {
    // count(const key_type&)
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    EXPECT(hm.count(1) == 1);
    EXPECT(hm.count(2) == 0);
    EXPECT(chm.count(1) == 1);
    EXPECT(chm.count(2) == 0);
  }

  {
    // template <class K> count(const K&)
    HashMap<int, int, Hash, Equal> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    EXPECT(hm.count("1") == 1);
    EXPECT(hm.count("2") == 0);
    EXPECT(chm.count("1") == 1);
    EXPECT(chm.count("2") == 0);
  }

  {
    // find(const key_type&)
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    {
      auto it = hm.find(1);
      EXPECT(it != hm.end());
      EXPECT(it->first == 1);
      EXPECT(it->second == 1);
      it = hm.find(2);
      EXPECT(it == hm.end());
    }
    {
      auto it = chm.find(1);
      EXPECT(it != chm.end());
      EXPECT(it->first == 1);
      EXPECT(it->second == 1);
      it = chm.find(2);
      EXPECT(it == chm.end());
    }
  }

  {
    // template <class K> find(const K&)
    HashMap<int, int, Hash, Equal> hm(16, 0);
    const auto &chm = hm;
    hm[1] = 1;
    {
      auto it = hm.find("1");
      EXPECT(it != hm.end());
      EXPECT(it->first == 1);
      EXPECT(it->second == 1);
      it = hm.find("2");
      EXPECT(it == hm.end());
    }
    {
      auto it = chm.find("1");
      EXPECT(it != chm.end());
      EXPECT(it->first == 1);
      EXPECT(it->second == 1);
      it = chm.find("2");
      EXPECT(it == chm.end());
    }
  }

  // Bucket interface
  {
    // bucket_count()
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    EXPECT(hm.bucket_count() == 16);
    EXPECT(chm.bucket_count() == 16);
  }

  {
    // max_bucket_count()
    HashMap<int, int> hm(16, 0);
    const auto &chm = hm;
    EXPECT(hm.max_bucket_count() > 0);
    EXPECT(chm.max_bucket_count() > 0);
  }

  // Hash policy
  {
    HashMap<int, int> hm(2, 0);
    const auto &chm = hm;
    hm.emplace(1, 1);
    hm.emplace(2, 2);
    EXPECT(hm.bucket_count() == 4);
    EXPECT(chm.bucket_count() == 4);
    hm.rehash(2);
    EXPECT(hm.bucket_count() == 4);
    EXPECT(chm.bucket_count() == 4);
    hm.rehash(16);
    EXPECT(hm.bucket_count() == 16);
    EXPECT(chm.bucket_count() == 16);
    hm.reserve(2);
    EXPECT(hm.bucket_count() == 16);
    EXPECT(chm.bucket_count() == 16);
    hm.reserve(16);
    EXPECT(hm.bucket_count() == 32);
    EXPECT(chm.bucket_count() == 32);
  }

  if (!ok) {
    fprintf(stderr, "FAILED!\n");
  }
  return !ok;
}
