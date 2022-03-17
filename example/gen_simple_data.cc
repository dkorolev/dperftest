// Synopsis: ./Debug/gen_simple_data -n 1000000 --output queries.txt --output_golden goldens.txt

#include <fstream>

#include "schema.h"

#include "bricks/dflags/dflags.h"
#include "typesystem/serialization/json.h"

DEFINE_uint32(n, 100, "The number of queries to generate.");
DEFINE_string(output, "/dev/stdout", "The name of the output file to write the queries to.");
DEFINE_string(output_golden, "/dev/null", "The name of the output file to write the golden results to.");
DEFINE_uint16(random_seed, 42, "The random seed to use.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  std::ofstream f_queries(FLAGS_output);
  std::ofstream f_goldens(FLAGS_output_golden);

  srand(FLAGS_random_seed);

  RequestBody request_body;
  ResponseBody response_body;

  for (uint32_t i = 0u; i < FLAGS_n; ++i) {
    request_body.a = 100 + (rand() % 400);
    request_body.b = 100 + (rand() % 400);
    response_body.c = Value(request_body.a) + Value(request_body.b);
    f_queries << JSON(request_body) << '\n';
    f_goldens << JSON(response_body) << '\n';
  }
}
