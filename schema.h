#pragma once

#include "typesystem/struct.h"

CURRENT_STRUCT(RequestBody) {
  CURRENT_FIELD(a, Optional<int32_t>);
  CURRENT_FIELD(b, Optional<int32_t>);
};

CURRENT_STRUCT(ResponseBody) {
  CURRENT_FIELD(c, int32_t);
  CURRENT_CONSTRUCTOR(ResponseBody)(int32_t c = 0) : c(c) {}
};
