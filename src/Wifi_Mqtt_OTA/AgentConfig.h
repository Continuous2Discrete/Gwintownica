#pragma once
#include "Topic.h"

// ==========================
// Lista parametrów procesu gwintowania
// X nazwa, typ, default, subscribable, publish_on_connect, topic
// ==========================
#define EDGE_PARAMETRY_LIST(X) \
  X(przyspieszenie_a,      int, 20000, true, true, Topic::PRZYSPIESZENIE_A) \
  X(v_bazowanie,           int, 3000,  true, true, Topic::V_BAZOWANIE) \
  X(v_szybki,              int, 35000, true, true, Topic::V_SZYBKI) \
  X(v_gwint,               int, 5200,  true, true, Topic::V_GWINT) \
  X(v_powrot,              int, 10900,  true, true, Topic::V_POWROT) \
  X(odjazd_od_kranc_kroki, int,   7000,      true, true, Topic::ODJAZD_KROKI) \
  X(poz_start_gwintu,      int,   84000,     true, true, Topic::POZ_START_GWINTU) \
  X(dlugosc_gwintu,        int,   55000,     true, true, Topic::DLUGOSC_GWINTU) \
  X(zapas_wyjazdu,         int,   30000,      true, true, Topic::ZAPAS_WYJAZDU)

// ==========================
// Wymagane makra z platformio.ini
// ==========================
#ifndef WIFI_SSID
  #error "WIFI_SSID must be defined"
#endif
#ifndef WIFI_PASS
  #error "WIFI_PASS must be defined"
#endif
#ifndef WIFI_HOSTNAME
  #define WIFI_HOSTNAME "ESP32-Aktualny_Projekt"
#endif
#ifndef MQTT_SERVER
  #error "MQTT_SERVER must be defined"
#endif
#ifndef MQTT_USER
  #error "MQTT_USER must be defined"
#endif
#ifndef MQTT_PASS
  #error "MQTT_PASS must be defined"
#endif
#ifndef MQTT_CLIENT_ID
  #error "MQTT_CLIENT_ID must be defined"
#endif

#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif

// ==========================
// Ustawienia komponentu
// ==========================
#ifndef EDGE_MAX_SUBS
  #define EDGE_MAX_SUBS 24
#endif
#ifndef EDGE_WIFI_RETRY_MS
  #define EDGE_WIFI_RETRY_MS 3000UL
#endif
#ifndef EDGE_MQTT_RETRY_MS
  #define EDGE_MQTT_RETRY_MS 3000UL
#endif
#ifndef EDGE_DEBUG
  #define EDGE_DEBUG 1
#endif

