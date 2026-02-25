#pragma once
#include <Arduino.h>
#include "Parametry.h"
#include "Wifi_Mqtt_OTA.h"

class Agent {
public:
  Agent();

  void begin();
  void loop();

  Parametry& parametry();

  // publikacja telemetrii i zmiennych procesowych
  bool publish(const char* topic, int value);
  bool publish(const char* topic, float value);
  bool publish(const char* topic, bool value);
  bool publish(const char* topic, const char* value);

  // <- PRZENIEŚ TO TU
  enum class ParamType : uint8_t { Bool, Int, Float };

  struct ParamDef {
    const char* topic;
    ParamType type;
    size_t offset;
    bool subscribable;
    bool publishOnConnect;
  };

private:
  NetMqttOta _net;
  Parametry _parametry;

  static const ParamDef _defs[];
  static const size_t _defsCount;

  static void _onMqttConnected(void* ctx);
  void _publishSnapshot();

  static void _onParamMsg(const char* topic, const uint8_t* payload, size_t len, void* ctx);
  void _handleParamMsg(const char* topic, const uint8_t* payload, size_t len);

  const ParamDef* _findDefByTopic(const char* topic) const;

  static bool _parseBool(const uint8_t* payload, size_t len, bool& out);
  static bool _parseInt(const uint8_t* payload, size_t len, int& out);
  static bool _parseFloat(const uint8_t* payload, size_t len, float& out);

  static void _toStr(char* buf, size_t bufSize, int v);
  static void _toStr(char* buf, size_t bufSize, float v);
  static void _toStr(char* buf, size_t bufSize, bool v);
};