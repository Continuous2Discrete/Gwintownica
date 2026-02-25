#include "Wifi_Mqtt_OTA.h"

NetMqttOta* NetMqttOta::_instance = nullptr;

#if EDGE_DEBUG
  #define NETLOGF(...) Serial.printf(__VA_ARGS__)
  #define NETLOGLN(x)  Serial.println(x)
#else
  #define NETLOGF(...)
  #define NETLOGLN(x)
#endif

NetMqttOta::NetMqttOta() : _mqtt(_wifi) {
  _instance = this;
}

void NetMqttOta::setOnMqttConnected(ConnectedHandler handler, void* ctx) {
  _onConnected = handler;
  _onConnectedCtx = ctx;
}

void NetMqttOta::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  _mqtt.setServer(MQTT_SERVER, (uint16_t)MQTT_PORT);
  _mqtt.setCallback(NetMqttOta::_mqttCallbackTrampoline);

  _ensureWifi();
}

void NetMqttOta::loop() {
  _ensureWifi();
  _ensureMqtt();

  if (_mqtt.connected()) {
    _mqtt.loop();
  }

  // OTA zawsze dostępne, ale handler działa tylko gdy OTA jest aktywowane komendą
  // U Ciebie OTA jest "wymagane", ale aktywacja może być po topicu MQTT_TOPIC_OTA
  ArduinoOTA.handle();
}

bool NetMqttOta::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool NetMqttOta::mqttConnected() {
  return _mqtt.connected();
}

bool NetMqttOta::publish(const char* topic, const char* payload) {
  if (!_mqtt.connected()) return false;
  return _mqtt.publish(topic, payload);
}

bool NetMqttOta::publish(const char* topic, const uint8_t* payload, size_t len) {
  if (!_mqtt.connected()) return false;
  return _mqtt.publish(topic, payload, len);
}

bool NetMqttOta::subscribe(const char* topic, MsgHandler handler, void* ctx) {
  if (!topic || !handler) return false;
  if (_subCount >= EDGE_MAX_SUBS) return false;

  _subs[_subCount].topic = topic;
  _subs[_subCount].handler = handler;
  _subs[_subCount].ctx = ctx;
  _subCount++;

  // real subscribe zrobi się po connect w _resubscribeAll
  return true;
}

void NetMqttOta::_ensureWifi() {
  if (wifiConnected()) return;

  const unsigned long now = millis();
  if (now - _lastWifiAttempt < EDGE_WIFI_RETRY_MS) return;
  _lastWifiAttempt = now;

  NETLOGF("[WiFi] Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void NetMqttOta::_ensureMqtt() {
  if (!wifiConnected()) return;
  if (_mqtt.connected()) return;

  const unsigned long now = millis();
  if (now - _lastMqttAttempt < EDGE_MQTT_RETRY_MS) return;
  _lastMqttAttempt = now;

  NETLOGF("[MQTT] Connecting to %s:%u ... ", MQTT_SERVER, (unsigned)MQTT_PORT);

  const bool ok = _mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);

  if (ok) {
    NETLOGLN("OK");

    // OTA setup raz
    if (!_otaConfigured) _setupOtaOnce();

    // 1) Najpierw snapshot parametrów i innych rzeczy z warstwy wyższej
    if (_onConnected) {
      _onConnected(_onConnectedCtx);
    }

    // 2) Dopiero potem subskrypcje
    _resubscribeAll();

  } else {
    NETLOGF("FAIL state=%d\n", _mqtt.state());
  }
}

void NetMqttOta::_setupOtaOnce() {
  _otaConfigured = true;

  ArduinoOTA.onStart([]() { NETLOGLN("[OTA] Start"); });
  ArduinoOTA.onEnd([]() { NETLOGLN("[OTA] End"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    if (t == 0) return;
    NETLOGF("[OTA] %u%%\r", (p * 100U) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    NETLOGF("[OTA] Error %u\n", (unsigned)e);
  });

  ArduinoOTA.begin();
  NETLOGLN("[OTA] Ready");
}

void NetMqttOta::_resubscribeAll() {
  // OTA topic always subscribed (required)
  _mqtt.subscribe(Topic::MQTT_TOPIC_OTA);

  for (size_t i = 0; i < _subCount; i++) {
    if (_subs[i].topic) {
      _mqtt.subscribe(_subs[i].topic);
    }
  }
}

void NetMqttOta::_mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int length) {
  if (_instance) _instance->_mqttCallback(topic, payload, length);
}

static bool payloadEquals(const uint8_t* p, size_t len, const char* s) {
  const size_t sl = strlen(s);
  return (len == sl) && memcmp(p, s, len) == 0;
}

void NetMqttOta::_mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic) return;

  // OTA control: MQTT_TOPIC_OTA required
  if (strcmp(topic, Topic::MQTT_TOPIC_OTA) == 0) {
    if (payloadEquals(payload, length, "ON")) {
      NETLOGLN("[OTA] Enabled by MQTT");
      // ArduinoOTA.handle i tak leci w loop(), a begin jest zawsze.
    } else {
      NETLOGLN("[OTA] Disabled by MQTT (noop)");
      // W tej wersji "disable" nie wyłącza ArduinoOTA, bo to komplikacja.
      // Twoje założenie: minimalnie, działa, bez fantazji.
    }
    return;
  }

  // Router SUB list
  for (size_t i = 0; i < _subCount; i++) {
    auto& s = _subs[i];
    if (!s.topic || !s.handler) continue;
    if (strcmp(topic, s.topic) == 0) {
      s.handler(topic, payload, (size_t)length, s.ctx);
      return;
    }
  }
}