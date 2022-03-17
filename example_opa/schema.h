#pragma once

#include "typesystem/struct.h"

CURRENT_STRUCT(RequestBodyOPAInput) {
  CURRENT_FIELD(role, std::string);
  CURRENT_FIELD(resource, std::string);
  CURRENT_FIELD(allowed, (std::unordered_map<std::string, std::set<std::string>>));
};

CURRENT_STRUCT(RequestBodyOPA) { CURRENT_FIELD(input, RequestBodyOPAInput); };

CURRENT_STRUCT(ResponseBodyOPA) {
  CURRENT_FIELD(result, bool);
  CURRENT_CONSTRUCTOR(ResponseBodyOPA)(bool result = false) : result(result) {}
};
