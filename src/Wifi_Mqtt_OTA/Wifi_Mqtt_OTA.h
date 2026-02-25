#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "AgentConfig.h"

class NetMqttOta {
public:
  using MsgHandler = void (*)(const char* topic, const uint8_t* payload, size_t len, void* ctx);
  using ConnectedHandler = void (*)(void* ctx);

  NetMqttOta();

  void begin();
  void loop();

  bool wifiConnected() const;
  bool mqttConnected();

  // SUB list
  bool subscribe(const char* topic, MsgHandler handler, void* ctx);

  // PUB
  bool publish(const char* topic, const char* payload); // bez retained
  bool publish(const char* topic, const uint8_t* payload, size_t len);

  // Hook po MQTT connect, wykonywany PRZED subskrypcjami
  void setOnMqttConnected(ConnectedHandler handler, void* ctx);

private:
  struct SubEntry {
    const char* topic = nullptr;
    MsgHandler handler = nullptr;
    void* ctx = nullptr;
  };

  WiFiClient _wifi;
  PubSubClient _mqtt;

  SubEntry _subs[EDGE_MAX_SUBS];
  size_t _subCount = 0;

  ConnectedHandler _onConnected = nullptr;
  void* _onConnectedCtx = nullptr;

  bool _otaConfigured = false;

  unsigned long _lastWifiAttempt = 0;
  unsigned long _lastMqttAttempt = 0;

  void _ensureWifi();
  void _ensureMqtt();

  void _setupOtaOnce();
  void _resubscribeAll();

  void _mqttCallback(char* topic, uint8_t* payload, unsigned int length);
  static void _mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int length);
  static NetMqttOta* _instance;
};