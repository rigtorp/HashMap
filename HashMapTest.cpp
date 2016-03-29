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
#include <cassert>

using namespace rigtorp;

int main(int argc, char *argv[]) {

  {
    HashMap<int, int> hm(16, 0);
    assert(hm.empty());
    assert(hm.size() == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.insert({1, 1});
    assert(!hm.empty());
    assert(hm.size() == 1);
    hm.clear();
    assert(hm.empty());
    assert(hm.size() == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.insert({1, 1});
    assert(res.second);
    assert(!hm.empty());
    assert(hm.size() == 1);
    assert(res.first->first == 1);
    assert(res.first->second == 1);
    res = hm.insert({1, 2});
    assert(!res.second);
    assert(hm.size() == 1);
    assert(res.first->first == 1);
    assert(res.first->second == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    assert(res.second);
    assert(!hm.empty());
    assert(hm.size() == 1);
    assert(res.first->first == 1);
    assert(res.first->second == 1);
    res = hm.emplace(1, 2);
    assert(!res.second);
    assert(hm.size() == 1);
    assert(res.first->first == 1);
    assert(res.first->second == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    auto res = hm.emplace(1, 1);
    assert(hm.size() == 1);
    hm.erase(res.first);
    assert(hm.size() == 0);
    assert(hm.erase(1) == 0);
    hm.emplace(1, 1);
    assert(hm.erase(1) == 1);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    assert(hm.at(1) == 1);
    hm.at(1) = 2;
    assert(hm.at(1) == 2);
    bool exception = false;
    try {
      hm.at(2);
    } catch (std::out_of_range) {
      exception = true;
    }
    assert(exception);
  }

  {
    HashMap<int, int> hm(16, 0);
    assert(hm[1] == 0);
    hm[1] = 2;
    assert(hm[1] == 2);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    assert(hm.count(1) == 1);
    assert(hm.count(2) == 0);
  }

  {
    HashMap<int, int> hm(16, 0);
    hm.emplace(1, 1);
    auto it = hm.find(1);
    assert(it != hm.end());
    assert(it->first == 1);
    assert(it->second == 1);
    it = hm.find(2);
    assert(it == hm.end());
  }

  {
    HashMap<int, int> hm(16, 0);
    assert(hm.bucket_count() == 16);
  }

  {
    HashMap<int, int> hm(2, 0);
    hm.emplace(1, 1);
    hm.emplace(2, 2);
    assert(hm.bucket_count() == 4);
    hm.rehash(2);
    assert(hm.bucket_count() == 4);
    hm.rehash(16);
    assert(hm.bucket_count() == 16);
    hm.reserve(2);
    assert(hm.bucket_count() == 16);
    hm.reserve(16);
    assert(hm.bucket_count() == 32);
  }

  return 0;
}
