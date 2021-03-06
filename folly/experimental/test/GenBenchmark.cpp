/*
 * Copyright 2013 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "folly/experimental/Gen.h"
#include "folly/experimental/StringGen.h"
#include "folly/experimental/FileGen.h"
#include "folly/String.h"

#include <atomic>
#include <thread>

#include <glog/logging.h>

#include "folly/Benchmark.h"

using namespace folly;
using namespace folly::gen;
using std::ostream;
using std::pair;
using std::set;
using std::vector;
using std::tuple;

static std::atomic<int> testSize(1000);
static vector<int> testVector =
    seq(1, testSize.load())
  | mapped([](int) { return rand(); })
  | as<vector>();

static vector<fbstring> testStrVector =
    seq(1, testSize.load())
  | eachTo<fbstring>()
  | as<vector>();

static vector<vector<int>> testVectorVector =
    seq(1, 100)
  | map([](int i) {
      return seq(1, i) | as<vector>();
    })
  | as<vector>();

auto square = [](int x) { return x * x; };
auto add = [](int a, int b) { return a + b; };
auto multiply = [](int a, int b) { return a * b; };

BENCHMARK(Sum_Basic_NoGen, iters) {
  int limit = testSize.load();
  int s = 0;
  while (iters--) {
    for (int i = 0; i < limit; ++i) {
      s += i;
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Sum_Basic_Gen, iters) {
  int limit = testSize.load();
  int s = 0;
  while (iters--) {
    s += range(0, limit) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Sum_Vector_NoGen, iters) {
  int s = 0;
  while (iters--) {
    for (auto& i : testVector) {
      s += i;
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Sum_Vector_Gen, iters) {
  int s = 0;
  while (iters--) {
    s += from(testVector) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Count_Vector_NoGen, iters) {
  int s = 0;
  while (iters--) {
    for (auto& i : testVector) {
      if (i * 2 < rand()) {
        ++s;
      }
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Count_Vector_Gen, iters) {
  int s = 0;
  while (iters--) {
    s += from(testVector)
       | filter([](int i) {
                  return i * 2 < rand();
                })
       | count;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Fib_Sum_NoGen, iters) {
  int s = 0;
  while (iters--) {
    auto fib = [](int limit) -> vector<int> {
      vector<int> ret;
      int a = 0;
      int b = 1;
      for (int i = 0; i * 2 < limit; ++i) {
        ret.push_back(a += b);
        ret.push_back(b += a);
      }
      return ret;
    };
    for (auto& v : fib(testSize.load())) {
      s += v;
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Fib_Sum_Gen, iters) {
  int s = 0;
  while (iters--) {
    auto fib = GENERATOR(int) {
      int a = 0;
      int b = 1;
      for (;;) {
        yield(a += b);
        yield(b += a);
      }
    };
    s += fib | take(testSize.load()) | sum;
  }
  folly::doNotOptimizeAway(s);
}

struct FibYielder {
  template<class Yield>
  void operator()(Yield&& yield) const {
    int a = 0;
    int b = 1;
    for (;;) {
      yield(a += b);
      yield(b += a);
    }
  }
};

BENCHMARK_RELATIVE(Fib_Sum_Gen_Static, iters) {
  int s = 0;
  while (iters--) {
    auto fib = generator<int>(FibYielder());
    s += fib | take(testSize.load()) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(VirtualGen_0Virtual, iters) {
  int s = 0;
  while (iters--) {
    auto numbers = seq(1, 10000);
    auto squares = numbers | map(square);
    auto quads = squares | map(square);
    s += quads | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(VirtualGen_1Virtual, iters) {
  int s = 0;
  while (iters--) {
    VirtualGen<int> numbers = seq(1, 10000);
    auto squares = numbers | map(square);
    auto quads = squares | map(square);
    s += quads | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(VirtualGen_2Virtual, iters) {
  int s = 0;
  while (iters--) {
    VirtualGen<int> numbers = seq(1, 10000);
    VirtualGen<int> squares = numbers | map(square);
    auto quads = squares | map(square);
    s += quads | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(VirtualGen_3Virtual, iters) {
  int s = 0;
  while (iters--) {
    VirtualGen<int> numbers = seq(1, 10000);
    VirtualGen<int> squares = numbers | map(square);
    VirtualGen<int> quads = squares | map(square);
    s += quads | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Concat_NoGen, iters) {
  int s = 0;
  while (iters--) {
    for (auto& v : testVectorVector) {
      for (auto& i : v) {
        s += i;
      }
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Concat_Gen, iters) {
  int s = 0;
  while (iters--) {
    s += from(testVectorVector) | rconcat | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Composed_NoGen, iters) {
  int s = 0;
  while (iters--) {
    for (auto& i : testVector) {
      s += i * i;
    }
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Composed_Gen, iters) {
  int s = 0;
  auto sumSq = map(square) | sum;
  while (iters--) {
    s += from(testVector) | sumSq;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(Composed_GenRegular, iters) {
  int s = 0;
  while (iters--) {
    s += from(testVector) | map(square) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(Sample, iters) {
  size_t s = 0;
  while (iters--) {
    auto sampler = seq(1, 10 * 1000 * 1000) | sample(1000);
    s += (sampler | sum);
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

namespace {

const char* const kLine = "The quick brown fox jumped over the lazy dog.\n";
const size_t kLineCount = 10000;
std::string bigLines;
const size_t kSmallLineSize = 17;
std::vector<std::string> smallLines;

void initStringResplitterBenchmark() {
  bigLines.reserve(kLineCount * strlen(kLine));
  for (size_t i = 0; i < kLineCount; ++i) {
    bigLines += kLine;
  }
  size_t remaining = bigLines.size();
  size_t pos = 0;
  while (remaining) {
    size_t n = std::min(kSmallLineSize, remaining);
    smallLines.push_back(bigLines.substr(pos, n));
    pos += n;
    remaining -= n;
  }
}

size_t len(folly::StringPiece s) { return s.size(); }

}  // namespace

BENCHMARK(StringResplitter_Big, iters) {
  size_t s = 0;
  while (iters--) {
    s += from({bigLines}) | resplit('\n') | map(&len) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringResplitter_Small, iters) {
  size_t s = 0;
  while (iters--) {
    s += from(smallLines) | resplit('\n') | map(&len) | sum;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(StringSplit_Old, iters) {
  size_t s = 0;
  std::string line(kLine);
  while (iters--) {
    std::vector<StringPiece> parts;
    split(' ', line, parts);
    s += parts.size();
  }
  folly::doNotOptimizeAway(s);
}


BENCHMARK_RELATIVE(StringSplit_Gen_Vector, iters) {
  size_t s = 0;
  StringPiece line(kLine);
  while (iters--) {
    s += (split(line, ' ') | as<vector>()).size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(StringSplit_Old_ReuseVector, iters) {
  size_t s = 0;
  std::string line(kLine);
  std::vector<StringPiece> parts;
  while (iters--) {
    parts.clear();
    split(' ', line, parts);
    s += parts.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringSplit_Gen_ReuseVector, iters) {
  size_t s = 0;
  StringPiece line(kLine);
  std::vector<StringPiece> parts;
  while (iters--) {
    parts.clear();
    split(line, ' ') | appendTo(parts);
    s += parts.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringSplit_Gen, iters) {
  size_t s = 0;
  StringPiece line(kLine);
  while (iters--) {
    s += split(line, ' ') | count;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringSplit_Gen_Take, iters) {
  size_t s = 0;
  StringPiece line(kLine);
  while (iters--) {
    s += split(line, ' ') | take(10) | count;
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK(StringUnsplit_Old, iters) {
  size_t s = 0;
  while (iters--) {
    fbstring joined;
    join(',', testStrVector, joined);
    s += joined.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringUnsplit_Old_ReusedBuffer, iters) {
  size_t s = 0;
  fbstring joined;
  while (iters--) {
    joined.clear();
    join(',', testStrVector, joined);
    s += joined.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringUnsplit_Gen, iters) {
  size_t s = 0;
  StringPiece line(kLine);
  while (iters--) {
    fbstring joined = from(testStrVector) | unsplit(',');
    s += joined.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_RELATIVE(StringUnsplit_Gen_ReusedBuffer, iters) {
  size_t s = 0;
  fbstring buffer;
  while (iters--) {
    buffer.clear();
    from(testStrVector) | unsplit(',', &buffer);
    s += buffer.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

void StringUnsplit_Gen(size_t iters, size_t joinSize) {
  std::vector<fbstring> v;
  BENCHMARK_SUSPEND {
    FOR_EACH_RANGE(i, 0, joinSize) {
      v.push_back(to<fbstring>(rand()));
    }
  }
  size_t s = 0;
  fbstring buffer;
  while (iters--) {
    buffer.clear();
    from(v) | unsplit(',', &buffer);
    s += buffer.size();
  }
  folly::doNotOptimizeAway(s);
}

BENCHMARK_DRAW_LINE()

BENCHMARK_PARAM(StringUnsplit_Gen, 1000)
BENCHMARK_RELATIVE_PARAM(StringUnsplit_Gen, 2000)
BENCHMARK_RELATIVE_PARAM(StringUnsplit_Gen, 4000)
BENCHMARK_RELATIVE_PARAM(StringUnsplit_Gen, 8000)

BENCHMARK_DRAW_LINE()

BENCHMARK(ByLine_Pipes, iters) {
  std::thread thread;
  int rfd;
  int wfd;
  BENCHMARK_SUSPEND {
    int p[2];
    CHECK_ERR(::pipe(p));
    rfd = p[0];
    wfd = p[1];
    thread = std::thread([wfd, iters] {
      char x = 'x';
      PCHECK(::write(wfd, &x, 1) == 1);  // signal startup
      FILE* f = fdopen(wfd, "w");
      PCHECK(f);
      for (int i = 1; i <= iters; ++i) {
        fprintf(f, "%d\n", i);
      }
      fclose(f);
    });
    char buf;
    PCHECK(::read(rfd, &buf, 1) == 1);  // wait for startup
  }

  auto s = byLine(rfd) | eachTo<int64_t>() | sum;
  folly::doNotOptimizeAway(s);

  BENCHMARK_SUSPEND {
    ::close(rfd);
    CHECK_EQ(s, int64_t(iters) * (iters + 1) / 2);
    thread.join();
  }
}

// Results from a dual core Xeon L5520 @ 2.27GHz:
//
// ============================================================================
// folly/experimental/test/GenBenchmark.cpp        relative  time/iter  iters/s
// ============================================================================
// Sum_Basic_NoGen                                            354.70ns    2.82M
// Sum_Basic_Gen                                     95.88%   369.92ns    2.70M
// ----------------------------------------------------------------------------
// Sum_Vector_NoGen                                           211.89ns    4.72M
// Sum_Vector_Gen                                    97.49%   217.35ns    4.60M
// ----------------------------------------------------------------------------
// Count_Vector_NoGen                                          13.93us   71.78K
// Count_Vector_Gen                                 106.38%    13.10us   76.36K
// ----------------------------------------------------------------------------
// Fib_Sum_NoGen                                                4.54us  220.07K
// Fib_Sum_Gen                                       45.81%     9.92us  100.82K
// Fib_Sum_Gen_Static                               100.00%     4.54us  220.05K
// ----------------------------------------------------------------------------
// VirtualGen_0Virtual                                         12.03us   83.14K
// VirtualGen_1Virtual                               32.89%    36.57us   27.34K
// VirtualGen_2Virtual                               24.98%    48.15us   20.77K
// VirtualGen_3Virtual                               17.82%    67.49us   14.82K
// ----------------------------------------------------------------------------
// Concat_NoGen                                                 1.92us  520.46K
// Concat_Gen                                       102.79%     1.87us  534.97K
// ----------------------------------------------------------------------------
// Composed_NoGen                                             545.64ns    1.83M
// Composed_Gen                                      99.65%   547.55ns    1.83M
// Composed_GenRegular                               99.64%   547.62ns    1.83M
// ----------------------------------------------------------------------------
// StringResplitter_Big                                       120.88us    8.27K
// StringResplitter_Small                            14.39%   839.94us    1.19K
// ----------------------------------------------------------------------------
// StringSplit_Old                                            421.09ns    2.37M
// StringSplit_Gen_Vector                            97.73%   430.87ns    2.32M
// ----------------------------------------------------------------------------
// StringSplit_Old_ReuseVector                                 80.25ns   12.46M
// StringSplit_Gen_ReuseVector                       98.99%    81.07ns   12.34M
// StringSplit_Gen                                  117.23%    68.45ns   14.61M
// StringSplit_Gen_Take                             115.23%    69.64ns   14.36M
// ----------------------------------------------------------------------------
// StringUnsplit_Old                                           34.45us   29.02K
// StringUnsplit_Old_ReusedBuffer                   100.37%    34.33us   29.13K
// StringUnsplit_Gen                                106.27%    32.42us   30.84K
// StringUnsplit_Gen_ReusedBuffer                   105.61%    32.62us   30.65K
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// StringUnsplit_Gen(1000)                                     32.20us   31.06K
// StringUnsplit_Gen(2000)                           49.41%    65.17us   15.34K
// StringUnsplit_Gen(4000)                           22.75%   141.52us    7.07K
// StringUnsplit_Gen(8000)                           11.20%   287.53us    3.48K
// ----------------------------------------------------------------------------
// ByLine_Pipes                                               126.58ns    7.90M
// ============================================================================

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  initStringResplitterBenchmark();
  runBenchmarks();
  return 0;
}
