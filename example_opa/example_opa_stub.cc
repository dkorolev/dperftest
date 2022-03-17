#include <iostream>

#include "schema.h"

#include "bricks/dflags/dflags.h"
#include "blocks/http/api.h"
#include "typesystem/serialization/json.h"

DEFINE_uint16(port, 8181, "The port to listen on.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

#ifndef NDEBUG
  std::cout << "Warning: running a DEBUG build. Suboptimal for performance testing." << std::endl;
#endif

  auto& http = HTTP(current::net::BarePort(FLAGS_port));
  auto const scope = http.Register("/v1/data/myapi/allow", [](Request r) {
    Optional<RequestBodyOPA> body = TryParseJSON<RequestBodyOPA>(r.body);
    bool result = false;
    if (Exists(body)) {
      RequestBodyOPAInput const& input = Value(body).input;
      auto const cit = input.allowed.find(input.resource);
      if (cit != input.allowed.end()) {
        result = cit->second.count(input.role);
      }
    }
    r(ResponseBodyOPA(result));
  });

  std::cout << "Example OPA server listening on port " << FLAGS_port << ". Try:" << std::endl;

  {
    RequestBodyOPA test_input;
    test_input.input.role = "foo";
    test_input.input.resource = "bar";
    test_input.input.allowed["bar"].insert("foo");
    std::cout << "curl -s -X POST http://localhost:" << FLAGS_port << "/v1/data/myapi/allow --data-binary '"
              << JSON(test_input) << "'" << std::endl;
  }

  http.Join();
}
