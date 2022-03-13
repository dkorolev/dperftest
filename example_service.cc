#include <iostream>

#include "bricks/dflags/dflags.h"
#include "blocks/http/api.h"
#include "typesystem/serialization/json.h"

CURRENT_STRUCT(RequestBody) {
  CURRENT_FIELD(a, Optional<int32_t>);
  CURRENT_FIELD(b, Optional<int32_t>);
};

CURRENT_STRUCT(ResponseBody) {
  CURRENT_FIELD(c, int32_t);
  CURRENT_CONSTRUCTOR(ResponseBody)(int32_t c) : c(c) {}
};

DEFINE_uint16(port, 8181, "The port to listen on.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
#ifdef NDEBUG
  std::cout << "Running an `NDEBUG` build." << std::endl;
#endif
  auto& http = HTTP(current::net::BarePort(FLAGS_port));
  auto const scope = http.Register("/", [](Request r) {
    int32_t result = 0;
    Optional<RequestBody> body = TryParseJSON<RequestBody>(r.body);
    if (Exists(body)) {
      RequestBody const& parsed = Value(body);
      if (Exists(parsed.a)) {
        result += Value(parsed.a);
      }
      if (Exists(parsed.b)) {
        result += Value(parsed.b);
      }
    }
    r(ResponseBody(result));
  });
  std::cout << "Example server listening on port " << FLAGS_port << ". Try:" << std::endl;
  std::cout << "curl http://localhost:" << FLAGS_port << " -d '{\"a\":3,\"b\":4}'  # => `{\"c\":7}`" << std::endl;
  http.Join();
}
