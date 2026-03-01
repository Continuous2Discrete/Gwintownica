#include "Agent.h"
#include "AgentConfig.h"

#if EDGE_DEBUG
  #define ALOGF(...) Serial.printf(__VA_ARGS__)
  #define ALOGLN(x)  Serial.println(x)
#else
  #define ALOGF(...)
  #define ALOGLN(x)
#endif

// ====== Generowanie tablicy definicji parametrów z EDGE_PARAMETRY_LIST ======
#define EDGE_PARAM_TYPE_BOOL  Agent::ParamType::Bool
#define EDGE_PARAM_TYPE_INT   Agent::ParamType::Int
#define EDGE_PARAM_TYPE_FLOAT Agent::ParamType::Float

template<typename T> struct _EdgeTypeMap;
template<> struct _EdgeTypeMap<bool>  { static constexpr Agent::ParamType t = EDGE_PARAM_TYPE_BOOL;  };
template<> struct _EdgeTypeMap<int>   { static constexpr Agent::ParamType t = EDGE_PARAM_TYPE_INT;   };
template<> struct _EdgeTypeMap<float> { static constexpr Agent::ParamType t = EDGE_PARAM_TYPE_FLOAT; };

// UWAGA: Definicje są "statyczne" i wewnętrzne, zgodnie z Twoim wymaganiem.
const Agent::ParamDef Agent::_defs[] = {
  #define EDGE_DEF_ENTRY(name, type, def, sub, pub, topic) \
    { topic, _EdgeTypeMap<type>::t, offsetof(Parametry, name), (sub), (pub) },
  EDGE_PARAMETRY_LIST(EDGE_DEF_ENTRY)
  #undef EDGE_DEF_ENTRY
};

const size_t Agent::_defsCount = sizeof(Agent::_defs) / sizeof(Agent::_defs[0]);

Agent::Agent() {}

void Agent::begin() {
  _parametry.loadDefaults();

  // Hook: po MQTT connect -> publish snapshot, a dopiero potem subskrypcje (robi NetMqttOta)
  _net.setOnMqttConnected(Agent::_onMqttConnected, this);

  // Rejestrujemy SUB listę: tylko parametry oznaczone subscribable
  for (size_t i = 0; i < _defsCount; i++) {
    if (_defs[i].subscribable) {
      _net.subscribe(_defs[i].topic, Agent::_onParamMsg, this);
    }
  }

  _net.begin();
}

void Agent::loop() {
  _net.loop();
}

Parametry& Agent::parametry() {
  return _parametry;
}

void Agent::_onMqttConnected(void* ctx) {
  auto* self = static_cast<Agent*>(ctx);
  self->_publishSnapshot();
}

void Agent::_publishSnapshot() {
  // Publikujemy tylko te z publishOnConnect
  // Bez retained, zgodnie z Twoim wymaganiem.
  _net.publish(Topic::MQTT_TOPIC_OTA, "OFF");
  char buf[48];

  for (size_t i = 0; i < _defsCount; i++) {
    const auto& d = _defs[i];
    if (!d.publishOnConnect) continue;

    const uint8_t* base = reinterpret_cast<const uint8_t*>(&_parametry);
    const void* ptr = base + d.offset;

    switch (d.type) {
      case ParamType::Bool: {
        const bool v = *reinterpret_cast<const bool*>(ptr);
        _toStr(buf, sizeof(buf), v);
        _net.publish(d.topic, buf);
      } break;
      case ParamType::Int: {
        const int v = *reinterpret_cast<const int*>(ptr);
        _toStr(buf, sizeof(buf), v);
        _net.publish(d.topic, buf);
      } break;
      case ParamType::Float: {
        const float v = *reinterpret_cast<const float*>(ptr);
        _toStr(buf, sizeof(buf), v);
        _net.publish(d.topic, buf);
      } break;
    }
  }
}

// ====== MQTT -> Parametry ======

void Agent::_onParamMsg(const char* topic, const uint8_t* payload, size_t len, void* ctx) {
  auto* self = static_cast<Agent*>(ctx);
  self->_handleParamMsg(topic, payload, len);
}

const Agent::ParamDef* Agent::_findDefByTopic(const char* topic) const {
  for (size_t i = 0; i < _defsCount; i++) {
    if (strcmp(topic, _defs[i].topic) == 0) return &_defs[i];
  }
  return nullptr;
}

void Agent::_handleParamMsg(const char* topic, const uint8_t* payload, size_t len) {
  const ParamDef* d = _findDefByTopic(topic);
  if (!d) return;

  uint8_t* base = reinterpret_cast<uint8_t*>(&_parametry);
  void* ptr = base + d->offset;

  switch (d->type) {
    case ParamType::Bool: {
      bool v = false;
      if (_parseBool(payload, len, v)) {
        *reinterpret_cast<bool*>(ptr) = v;
      }
    } break;
    case ParamType::Int: {
      int v = 0;
      if (_parseInt(payload, len, v)) {
        *reinterpret_cast<int*>(ptr) = v;
      }
    } break;
    case ParamType::Float: {
      float v = 0.f;
      if (_parseFloat(payload, len, v)) {
        *reinterpret_cast<float*>(ptr) = v;
      }
    } break;
  }
}

// ====== Publish API ======

bool Agent::publish(const char* topic, int value) {
  char buf[32];
  _toStr(buf, sizeof(buf), value);
  return _net.publish(topic, buf);
}

bool Agent::publish(const char* topic, float value) {
  char buf[48];
  _toStr(buf, sizeof(buf), value);
  return _net.publish(topic, buf);
}

bool Agent::publish(const char* topic, bool value) {
  char buf[8];
  _toStr(buf, sizeof(buf), value);
  return _net.publish(topic, buf);
}

bool Agent::publish(const char* topic, const char* value) {
  return _net.publish(topic, value);
}

// ====== Parsowanie ======

static bool _payloadEquals(const uint8_t* p, size_t len, const char* s) {
  const size_t sl = strlen(s);
  return len == sl && memcmp(p, s, len) == 0;
}

bool Agent::_parseBool(const uint8_t* payload, size_t len, bool& out) {
  // przyjmujemy prosto: 1/0, ON/OFF, true/false
  if (_payloadEquals(payload, len, "1") || _payloadEquals(payload, len, "ON") || _payloadEquals(payload, len, "true")) {
    out = true; return true;
  }
  if (_payloadEquals(payload, len, "0") || _payloadEquals(payload, len, "OFF") || _payloadEquals(payload, len, "false")) {
    out = false; return true;
  }
  return false;
}

bool Agent::_parseInt(const uint8_t* payload, size_t len, int& out) {
  char buf[32];
  if (len == 0 || len >= sizeof(buf)) return false;
  memcpy(buf, payload, len);
  buf[len] = '\0';
  char* end = nullptr;
  long v = strtol(buf, &end, 10);
  if (end == buf) return false;
  out = (int)v;
  return true;
}

bool Agent::_parseFloat(const uint8_t* payload, size_t len, float& out) {
  char buf[32];
  if (len == 0 || len >= sizeof(buf)) return false;
  memcpy(buf, payload, len);
  buf[len] = '\0';
  char* end = nullptr;
  float v = strtof(buf, &end);
  if (end == buf) return false;
  out = v;
  return true;
}

// ====== Formatowanie ======

void Agent::_toStr(char* buf, size_t bufSize, int v) {
  snprintf(buf, bufSize, "%d", v);
}

void Agent::_toStr(char* buf, size_t bufSize, float v) {
  // prosto: 3 miejsca po przecinku (zmień w razie potrzeby)
  snprintf(buf, bufSize, "%.3f", (double)v);
}

void Agent::_toStr(char* buf, size_t bufSize, bool v) {
  snprintf(buf, bufSize, "%s", v ? "1" : "0");
}