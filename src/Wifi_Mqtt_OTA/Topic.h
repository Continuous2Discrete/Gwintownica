#pragma once

// Wszystkie topic'i w jednym miejscu.
// Używamy constexpr zamiast #define: typowane, bezpieczniejsze, bez konfliktów makr.

#define MQTT_MAIN_NAME "ESP32-Gwintownica"

namespace Topic {
   // OTA
  inline constexpr const char* MQTT_TOPIC_OTA = MQTT_MAIN_NAME "/OTA";

  // Parametry procesu gwintowania do strojenia
  inline constexpr const char* PRZYSPIESZENIE_A      = MQTT_MAIN_NAME "/param/a";
  inline constexpr const char* V_BAZOWANIE           = MQTT_MAIN_NAME "/param/v_bazowanie";
  inline constexpr const char* V_SZYBKI              = MQTT_MAIN_NAME "/param/v_szybki";
  inline constexpr const char* V_GWINT               = MQTT_MAIN_NAME "/param/v_gwint";
  inline constexpr const char* V_POWROT              = MQTT_MAIN_NAME "/param/v_powrot";

  inline constexpr const char* ODJAZD_KROKI          = MQTT_MAIN_NAME "/param/odjazd_od_krancowki";
  inline constexpr const char* POZ_START_GWINTU      = MQTT_MAIN_NAME "/param/poz_start_gwintu";
  inline constexpr const char* DLUGOSC_GWINTU        = MQTT_MAIN_NAME "/param/dlugosc_gwintu";
  inline constexpr const char* ZAPAS_WYJAZDU         = MQTT_MAIN_NAME "/param/zapas_wyjazdu";

  // Telemetria raportowa — publikujesz, nie subskrybujesz
  inline constexpr const char* POZYCJA               = MQTT_MAIN_NAME "/tele/pozycja";
  inline constexpr const char* SPEED                 = MQTT_MAIN_NAME "/tele/speed";
  inline constexpr const char* STATE                 = MQTT_MAIN_NAME "/tele/state";

}


