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
#include "HashMap2.h"
#include <chrono>
#include <google/dense_hash_map>
#include <iostream>
#include <random>
#include <unordered_map>

#include <benchmark/benchmark.h>

using namespace rigtorp;
using namespace std::chrono;

struct Hash {
  size_t operator()(uint64_t h) const noexcept {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
  }
};

constexpr size_t maxRange = 1000000;

using value = std::array<char, 32>;

template <class T> static void BM_LookupHit(benchmark::State &state) {
  auto hm = T(state.range(0));
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = 0;
  for (auto _ : state) {
    hm.find(i);
    ++i;
    if (i >= state.range(0)) {
      i = 0;
    }
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

template <class T> static void BM_LookupHit2(benchmark::State &state) {
  auto hm = T(state.range(0), INT32_MAX);
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = 0;
  for (auto _ : state) {
    hm.find(i);
    ++i;
    if (i >= state.range(0)) {
      i = 0;
    }
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

BENCHMARK_TEMPLATE(BM_LookupHit, std::unordered_map<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_LookupHit, HashMap2<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_LookupHit2, HashMap<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);

template <class T> static void BM_LookupMiss(benchmark::State &state) {
  T hm;
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = state.range(0);
  for (auto _ : state) {
    hm.find(i);
    ++i;
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

template <class T> static void BM_LookupMiss2(benchmark::State &state) {
  T hm(state.range(0), INT32_MAX);
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = state.range(0);
  for (auto _ : state) {
    hm.find(i);
    ++i;
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

BENCHMARK_TEMPLATE(BM_LookupMiss, std::unordered_map<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_LookupMiss, HashMap2<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_LookupMiss2, HashMap<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);

template <class T> static void BM_insert(benchmark::State &state) {
  T hm;
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = state.range(0);
  for (auto _ : state) {
    const auto it = hm.insert({i, {}}).first;
    // state.PauseTiming();
    hm.erase(it);
    ++i;
    // state.ResumeTiming();
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

template <class T> static void BM_insert2(benchmark::State &state) {
  T hm(0, INT32_MAX);
  for (int i = 0; i < state.range(0); ++i) {
    hm.insert({i, {}});
  }
  int i = state.range(0);
  for (auto _ : state) {
    const auto it = hm.insert({i, {}}).first;
    // state.PauseTiming();
    hm.erase(it);
    ++i;
    // state.ResumeTiming();
  }
  state.counters["LoadFactor"] = benchmark::Counter(hm.load_factor());
}

BENCHMARK_TEMPLATE(BM_insert, std::unordered_map<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_insert2, HashMap<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);
BENCHMARK_TEMPLATE(BM_insert, HashMap2<int, value, Hash>)
    ->RangeMultiplier(10)
    ->Range(10, maxRange);

BENCHMARK_MAIN()
