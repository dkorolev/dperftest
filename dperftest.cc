#include <iostream>
#include <sstream>
#include <mutex>
#include <atomic>

#include "bricks/dflags/dflags.h"
#include "bricks/strings/printf.h"
#include "bricks/strings/rounding.h"
#include "blocks/http/api.h"
#include "blocks/xterm/vt100.h"
#include "blocks/xterm/progress.h"

DEFINE_string(url, "", "The URL to POST the queries to.");
DEFINE_string(queries, "", "The input queries file, one POST body per line.");
DEFINE_string(content_type, "", "Set to pass `-H Content-Type: ...`, ex. `application/json`.");
DEFINE_string(goldens, "", "The input golden results file, optional.");
DEFINE_string(write_goldens, "", "The output file to write the goldens to, optional.");
DEFINE_uint32(threads, 16, "The number of threads to use, 16 should be a safe default.");
DEFINE_double(seconds, 5.0, "The number of seconds to run the test for.");
DEFINE_uint32(max_errors, 5u, "The number of first erroneous results to report.");
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

  if (i1 > i0) {
    size_t const total_queries = datapoints[i1].queries_processed - datapoints[i0].queries_processed;
    double const total_seconds = 1e-6 * (datapoints[i1].timestamp - datapoints[i0].timestamp).count();

    if (total_queries > 0 && total_seconds > 0) {
      std::ostringstream oss;
      oss << bold << green << current::strings::RoundDoubleToString(total_queries / total_seconds, 3) << " QPS"
          << reset;
      return oss.str();
    }
  }

  return "no long enough stable run to compute QPS";
}

inline int Run() {
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

  std::mutex cerr_mutex;
  bool error_logged = false;

  std::vector<Datapoint> datapoints;

  {
    current::ProgressLine report;

    for (uint32_t t = 0u; t < FLAGS_threads; ++t) {
      threads.emplace_back([&]() {
        try {
          ++atomic_active_threads;
          while (!error_logged) {
            size_t const i = atomic_index++;
            if (i >= queries.size()) {
              return;
            }
            size_t const actual_i = indexes[i];
            results[actual_i] =
                std::move(current::strings::Trim(HTTP(POST(FLAGS_url, queries[actual_i], FLAGS_content_type)).body));
          }
        } catch (current::net::SocketException const& e) {
          std::lock_guard<std::mutex> lock(cerr_mutex);
          if (!error_logged) {
            error_logged = true;
            atomic_index = N;
            report << "";
            std::cerr << bold << red << "ERROR" << reset << ": Could not make the POST request." << std::endl;
          }
        } catch (std::exception const& e) {
          std::lock_guard<std::mutex> lock(cerr_mutex);
          if (!error_logged) {
            error_logged = true;
            atomic_index = N;
            report << "";
            std::cerr << bold << red << "ERROR" << reset << ": " << e.what() << std::endl;
          }
        }
        --atomic_active_threads;
      });
    }

    if (error_logged) {
      for (uint32_t t = 0u; t < FLAGS_threads; ++t) {
        threads[t].join();
      }
      return 1;
    }

    while (atomic_index < queries.size()) {
      datapoints.push_back(
          Datapoint{SteadyNow(), std::min(static_cast<size_t>(atomic_index), N), atomic_active_threads});
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      report << ProgressReport(start_timestamp, queries.size(), datapoints) << ", " << QPSReport(datapoints) << '.';
    }
  }

  for (uint32_t t = 0u; t < FLAGS_threads; ++t) {
    threads[t].join();
  }

  if (!error_logged) {
    std::cout << "Done, " << QPSReport(datapoints) << " on " << bold << yellow << N << reset << " total queries."
              << std::endl;

    if (FLAGS_goldens.empty()) {
      if (FLAGS_write_goldens.empty()) {
        std::cout << "Not comparing the results against the goldens as `--goldens` were not provided." << std::endl;
      }
    } else {
      if (!FLAGS_write_goldens.empty()) {
        std::cout << "Do not set both `--goldens` and `--write_goldens`." << std::endl;
      }
      std::vector<std::string> const goldens =
          current::strings::Split(current::FileSystem::ReadFileAsString(FLAGS_goldens), '\n');
      if (goldens.size() != queries.size()) {
        std::cout << bold << yellow << "WARNING" << reset << ": `--queries` contain " << queries.size()
                  << " lines, while `--goldens` contain " << bold << red << goldens.size() << reset << " lines."
                  << std::endl;
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
          std::cout << bold << green << "PASSED" << reset << ", results match the goldens on " << magenta << M << reset
                    << " queries." << std::endl;
        } else {
          std::cout << bold << red << "FAILED" << reset << ", mismatch on " << red << errors_count << reset
                    << " out of " << magenta << M << reset << " queries, diff in line " << cyan << JSON(errors) << reset
                    << '.' << std::endl;
          return 1;
        }
      }
    }

    if (!FLAGS_write_goldens.empty()) {
      if (!FLAGS_goldens.empty()) {
        std::cout << "Do not set both `--goldens` and `--write_goldens`." << std::endl;
      } else {
        std::ofstream fo(FLAGS_write_goldens);
        if (!fo.good()) {
          std::cerr << bold << red << "ERROR" << reset << ": Can not write `" << FLAGS_write_goldens << "`."
                    << std::endl;
          return 1;
        }
        bool has_empty = false;
        bool has_multiline = false;
        for (std::string const& s : results) {
          std::vector<std::string> golden = current::strings::Split(s, '\n');
          if (golden.size() == 1u) {
            fo << golden.front() << '\n';
          } else if (golden.empty()) {
            fo << "[EMPTY]\n";
            has_empty = true;
          } else {
            fo << "[MULTILINE]" << current::strings::Join(golden, "\\n");
            has_multiline = true;
          }
        }
        std::cout << "Saved " << bold << yellow << "golden" << reset << " results to `" << FLAGS_write_goldens << "`."
                  << std::endl;
        if (has_empty) {
          std::cout << bold << yellow << "WARNING" << reset << ": Empty responses seen, replaced by `[EMPTY]`."
                    << std::endl;
        }
        if (has_multiline) {
          std::cout << bold << yellow << "WARNING" << reset
                    << ": Multi-line responses seen, prepended by `[MULTILINE]` and joined in golden output via `\\n`."
                    << std::endl;
        }
      }
    }

    return 0;
  } else {
    return 1;
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

#ifndef NDEBUG
  std::cout << bold << yellow << "WARNING" << reset << ": running a " << bold << red << "DEBUG" << reset
            << " build. Suboptimal for performance testing." << std::endl;
#endif

  try {
    return Run();
  } catch (current::net::SocketException const& e) {
    std::cerr << bold << red << "ERROR" << reset << ": Could not make the POST request." << std::endl;
  } catch (current::CannotReadFileException const& e) {
    std::cerr << bold << red << "ERROR" << reset << ": Could not read `" << e.OriginalDescription() << "`."
              << std::endl;
  } catch (std::exception const& e) {
    std::cerr << bold << red << "ERROR" << reset << ": " << e.what() << std::endl;
    return -1;
  }
}
