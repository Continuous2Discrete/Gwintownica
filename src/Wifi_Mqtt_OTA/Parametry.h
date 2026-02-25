#pragma once
#include <Arduino.h>
#include "AgentConfig.h"

struct Parametry {
  // Pola generowane z listy
  #define EDGE_DECL_FIELD(name, type, def, sub, pub, topic) type name;
  EDGE_PARAMETRY_LIST(EDGE_DECL_FIELD)
  #undef EDGE_DECL_FIELD

  void loadDefaults() {
    #define EDGE_SET_DEFAULT(name, type, def, sub, pub, topic) this->name = (type)(def);
    EDGE_PARAMETRY_LIST(EDGE_SET_DEFAULT)
    #undef EDGE_SET_DEFAULT
  }
};