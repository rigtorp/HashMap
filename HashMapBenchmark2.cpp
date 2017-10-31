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

static void BM_insert(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    HashMap<int, int, Hash> hm(0, INT32_MAX);
    state.ResumeTiming();
    for (int i = 0; i < state.range(0); ++i) {
      hm.insert({i, i});
    }
  }
  state.SetItemsProcessed(state.range(0));
  state.counters["FooRate"] =
      benchmark::Counter(state.range(0), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_insert)->RangeMultiplier(10)->Range(10, 100000000);

static void BM_insert2(benchmark::State &state) {
  for (auto _ : state) {
    state.PauseTiming();
    std::unordered_map<int, int, Hash> hm;
    state.ResumeTiming();
    for (int i = 0; i < state.range(0); ++i) {
      hm.insert({i, i});
    }
  }
  state.SetItemsProcessed(state.range(0));
  state.counters["FooRate"] =
      benchmark::Counter(state.range(0), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_insert2)->RangeMultiplier(10)->Range(10, 100000000);

BENCHMARK_MAIN()
