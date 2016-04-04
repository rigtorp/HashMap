/*
Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>

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

#include "HashMap.h"

using namespace rigtorp;

static bool ok = true;

#define EXPECT(expr)                                                           \
  ([](bool res) {                                                              \
    fprintf(stdout, "[%s] %s:%i: %s\n", res ? " OK " : "FAILED", __FILE__,     \
            __LINE__, #expr);                                                  \
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

int main(int argc, char *argv[]) {

  {
    HashMap<int, int> hm(16, 0);
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.insert({1, 1});
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    hm.clear();
    EXPECT(hm.empty());
    EXPECT(hm.size() == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.insert({1, 1});
    EXPECT(res.second);
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
    res = hm.insert({1, 2});
    EXPECT(!res.second);
    EXPECT(hm.size() == 1);
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    EXPECT(res.second);
    EXPECT(!hm.empty());
    EXPECT(hm.size() == 1);
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
    res = hm.emplace(1, 2);
    EXPECT(!res.second);
    EXPECT(hm.size() == 1);
    EXPECT(res.first->first == 1);
    EXPECT(res.first->second == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    EXPECT(hm.size() == 1);
    hm.erase(res.first);
    EXPECT(hm.size() == 0);
    EXPECT(hm.erase(1) == 0);
    hm.emplace(1, 1);
    EXPECT(hm.erase(1) == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    EXPECT(hm.at(1) == 1);
    hm.at(1) = 2;
    EXPECT(hm.at(1) == 2);
    EXPECT(THROWS(hm.at(2)));
  }

  {
    HashMap<int, int> hm(16, 0);
    EXPECT(hm[1] == 0);
    hm[1] = 2;
    EXPECT(hm[1] == 2);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    EXPECT(hm.count(1) == 1);
    EXPECT(hm.count(2) == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    auto it = hm.find(1);
    EXPECT(it != hm.end());
    EXPECT(it->first == 1);
    EXPECT(it->second == 1);
    it = hm.find(2);
    EXPECT(it == hm.end());
  }

  {
    HashMap<int, int> hm(16, 0);
    EXPECT(hm.bucket_count() == 16);
  }

  {
    HashMap<int, int> hm(2, 0);
    hm.emplace(1, 1);
    hm.emplace(2, 2);
    EXPECT(hm.bucket_count() == 4);
    hm.rehash(2);
    EXPECT(hm.bucket_count() == 4);
    hm.rehash(16);
    EXPECT(hm.bucket_count() == 16);
    hm.reserve(2);
    EXPECT(hm.bucket_count() == 16);
    hm.reserve(16);
    EXPECT(hm.bucket_count() == 32);
  }

  if (!ok) {
    fprintf(stderr, "FAILED!\n");
  }
  return !ok;
}
