#include <iostream>
#include <sstream>
#include <atomic>

#include "bricks/dflags/dflags.h"
#include "bricks/strings/printf.h"
#include "bricks/strings/rounding.h"
#include "blocks/http/api.h"
#include "blocks/xterm/vt100.h"
#include "blocks/xterm/progress.h"

DEFINE_string(url, "", "The URL to POST the queries to.");
DEFINE_string(queries, "", "The input queries file, one POST body per line.");
DEFINE_string(goldens, "", "The input golden results file, optional.");
DEFINE_uint32(threads, 16, "The number of threads to use, 16 should be a safe default.");
DEFINE_double(seconds, 5.0, "The number of seconds to run the test for.");
DEFINE_uint32(max_errors, 5u, "The number of first erorneus results to report.");
DEFINE_uint64(shuffle_random_seed, 42, "The random seed to use to randomize the order of queries.");

using namespace current::vt100;

struct Datapoint final {
  std::chrono::microseconds timestamp;
  size_t queries_processed;
  size_t active_threads;
};

inline std::chrono::microseconds SteadyNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

inline std::string ProgressReport(std::chrono::microseconds start_timestamp,
                                  size_t total_queries,
                                  std::vector<Datapoint> const& datapoints) {
  std::ostringstream oss;
  double const elapsed_seconds = 1e-6 * (datapoints.back().timestamp - start_timestamp).count();
  size_t const n = std::min(datapoints.back().queries_processed, total_queries);
  oss << "Done " << yellow << bold << current::strings::Printf("%.1lf%%", 100.0 * n / total_queries) << reset << " ("
      << n << " / " << total_queries << ") queries in " << blue << bold
      << current::strings::Printf("%.1lf", elapsed_seconds) << reset << " seconds";

  return oss.str();
}

inline std::string QPSReport(std::vector<Datapoint> const& datapoints) {
  size_t i0 = 0u;
  while (i0 < datapoints.size() && datapoints[i0].active_threads != FLAGS_threads) {
    ++i0;
  }
  size_t i1 = i0;
  while ((i1 + 1u) < datapoints.size() && datapoints[i1 + 1u].active_threads == FLAGS_threads) {
    ++i1;
  }

  size_t const total_queries = datapoints[i1].queries_processed - datapoints[i0].queries_processed;
  double const total_seconds = 1e-6 * (datapoints[i1].timestamp - datapoints[i0].timestamp).count();

  if (total_queries > 0 && total_seconds > 0) {
    std::ostringstream oss;
    oss << bold << green << current::strings::RoundDoubleToString(total_queries / total_seconds, 3) << "QPS" << reset;
    return oss.str();
  } else {
    return "no long enough stable run to compute QPS";
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

#ifndef NDEBUG
  std::cout << "Warning: running a " << bold << red << "DEBUG" << reset << " build. Suboptimal for performance testing."
            << std::endl;
#endif

  if (FLAGS_url.empty()) {
    std::cout << "The `--url` parameter is required." << std::endl;
    return 1;
  }

  if (FLAGS_queries.empty()) {
    std::cout << "The `--queries` parameter is required." << std::endl;
    return 1;
  }

  std::vector<std::string> const queries =
      current::strings::Split(current::FileSystem::ReadFileAsString(FLAGS_queries), '\n');
  size_t const N = queries.size();
  std::vector<std::string> results(N);

  std::vector<size_t> indexes(N);
  for (size_t i = 0u; i < N; ++i) {
    indexes[i] = i;
  }

  std::mt19937 g(FLAGS_shuffle_random_seed);
  std::shuffle(std::begin(indexes), std::end(indexes), g);

  std::atomic_size_t atomic_index(0u);
  std::atomic_size_t atomic_active_threads(0u);

  std::vector<std::thread> threads;
  threads.reserve(FLAGS_threads);

  std::chrono::microseconds const start_timestamp = SteadyNow();

  for (uint32_t t = 0u; t < FLAGS_threads; ++t) {
    threads.emplace_back([&]() {
      ++atomic_active_threads;
      while (true) {
        size_t const i = atomic_index++;
        if (i >= queries.size()) {
          return;
        }
        size_t const actual_i = indexes[i];
        results[actual_i] = std::move(current::strings::Trim(HTTP(POST(FLAGS_url, queries[actual_i])).body));
      }
      --atomic_active_threads;
    });
  }

  std::vector<Datapoint> datapoints;

  {
    current::ProgressLine report;
    while (atomic_index < queries.size()) {
      datapoints.push_back(
          Datapoint{SteadyNow(), std::min(static_cast<size_t>(atomic_index), N), atomic_active_threads});
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      report << ProgressReport(start_timestamp, queries.size(), datapoints) << ", " << QPSReport(datapoints) << '.';
    }
  }
  std::cout << "Done, " << QPSReport(datapoints) << " on " << bold << yellow << N << reset << " total queries."
            << std::endl;

  for (uint32_t t = 0u; t < FLAGS_threads; ++t) {
    threads[t].join();
  }

  if (FLAGS_goldens.empty()) {
    std::cout << "Not comparing the results against the goldens as `--goldens` were not provided." << std::endl;
  } else {
    std::vector<std::string> const goldens =
        current::strings::Split(current::FileSystem::ReadFileAsString(FLAGS_goldens), '\n');
    if (goldens.size() != queries.size()) {
      std::cout << "Warning: `--queries` contains " << queries.size() << " lines, while `--goldens` contains " << bold
                << red << goldens.size() << reset << " lines." << std::endl;
    }
    size_t const M = std::min(queries.size(), goldens.size());
    if (M) {
      std::vector<size_t> errors;
      size_t errors_count = 0u;
      for (size_t i = 0u; i < M; ++i) {
        if (current::strings::Trim(goldens[i]) != current::strings::Trim(results[i])) {
          ++errors_count;
          if (errors.size() < FLAGS_max_errors) {
            errors.push_back(i + 1u);
          }
        }
      }
      if (!errors_count) {
        std::cout << bold << green << "PASSED" << reset << ", results match on " << magenta << M << reset << " queries."
                  << std::endl;
      } else {
        std::cout << bold << red << "FAILED" << reset << ", mismatch on " << red << errors_count << reset << " out of "
                  << magenta << M << reset << " queries, deltas in queries " << cyan << JSON(errors) << reset << '.'
                  << std::endl;
      }
    }
  }
}
