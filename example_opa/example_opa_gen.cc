#include "schema.h"

#include "bricks/dflags/dflags.h"
#include "bricks/util/random.h"
#include "bricks/file/file.h"
#include "typesystem/serialization/json.h"

DEFINE_uint64(n, 10u, "How many requests to generate.");
DEFINE_uint16(k, 5u, "How many letters to use, to generate `k!` input strings.");
DEFINE_uint32(m, 50u, "The number of keys in `.allowed` per test.");
DEFINE_uint32(q, 50u, "The number of values for each key in `.allowed per test.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (!(FLAGS_k >= 2 && FLAGS_k <= 10)) {
    std::cerr << "You really want a small '--k`." << std::endl;
    std::exit(-1);
  }

  std::string s(FLAGS_k, '.');
  for (uint32_t i = 0u; i < FLAGS_k; ++i) {
    s[i] = 'A' + i;
  }

  std::vector<std::string> strings;
  do {
    strings.push_back(s);
  } while (std::next_permutation(std::begin(s), std::end(s)));

  std::random_device rd;
  std::mt19937 g(rd());
  std::uniform_int_distribution<size_t> rng(0u, strings.size() - 1u);

  for (uint64_t t = 0u; t < FLAGS_n; ++t) {
    std::shuffle(std::begin(strings), std::end(strings), g);
    RequestBodyOPA wrapped_input;
    auto& input = wrapped_input.input;
    input.role = strings[rng(rd)];
    input.resource = strings[rng(rd)];
    for (uint32_t i = 0u; i < FLAGS_k; ++i) {
      auto& allowed_roles_per_resource = input.allowed[strings[rng(rd)]];
      for (uint32_t i = 0u; i < FLAGS_q; ++i) {
        allowed_roles_per_resource.insert(strings[rng(rd)]);
      }
    }
    std::cout << JSON(wrapped_input) << std::endl;
  }
}
