#pragma once

#include <stdint.h>
#include <Arduino.h>

// ======================================================
// PINY (ESP32 Wemos D1 mini)
// ======================================================
static constexpr uint8_t PIN_STEP     = 25;
static constexpr uint8_t PIN_DIR      = 26;

static constexpr uint8_t PIN_BTN      = 27;  // INPUT_PULLUP, przycisk zwiera do GND (aktywny LOW)
static constexpr uint8_t PIN_KR_POCZ  = 32;  // krancowka poczatkowa aktywna HIGH
static constexpr uint8_t PIN_KR_KON   = 33;  // krancowka koncowa aktywna HIGH

// ======================================================
// LEDC (ESP-IDF) - generacja STEP (PWM)
// ======================================================
#include "driver/ledc.h"

// Uwagi:
// - w ruchu: czestotliwosc LEDC = Hz sygnalu STEP = kroki/s
// - 1000 impulsow = 1 obrot (wg Twojego opisu)

static constexpr ledc_mode_t    LEDC_MODE    = LEDC_HIGH_SPEED_MODE;
static constexpr ledc_timer_t   LEDC_TIMER   = LEDC_TIMER_0;
static constexpr ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0;

// Zakres czestotliwosci STEP
static constexpr uint32_t MIN_FREQ_STEP = 30;      // ponizej -> w praktyce lepiej STOP
static constexpr uint32_t MAX_FREQ_STEP = 100000;  // 100 kHz ~ max rpm wg Twojego napedu

// ======================================================
// PCNT (NOWE API - pulse_cnt) - liczenie krokow odcinka
// ======================================================
#include "driver/pulse_cnt.h"

// (opcjonalnie) filtr glitch - jesli chcesz wlaczyc, dodaj w OsRuchu.cpp
// static constexpr int PCNT_GLTICH_NS = 1000;

// ======================================================
// WEJSCIA - debounce
// ======================================================
static constexpr uint32_t DEBOUNCE_BTN_MS = 40;
static constexpr uint32_t DEBOUNCE_KR_MS = 20;

// ======================================================
// RUCH - RAMPA
// ======================================================
// Jedno przyspieszenie dla wszystkich zmian predkosci.
// Jednostki: kroki/s^2
static constexpr float PRZYSPIESZENIE_A = 25000.0f;

// Tick rampy (wywolywany w loop)
static constexpr uint32_t TICK_RAMPA_MS = 1;

// ======================================================
// TECHNOLOGIA - parametry procesu (DO STROJENIA)
// ======================================================

// Homing
static constexpr uint32_t ODJAZD_OD_KRANCOWKI_KROKI = 200;

// Pozycje (w krokach, absolutne)
static constexpr int32_t POZ_ZERO         = 0;
static constexpr int32_t POZ_START_GWINTU = 5000;   // przyklad: dojazd szybki do startu gwintowania

// Dystanse procesu (w krokach)
static constexpr int32_t DLUGOSC_GWINTU   = 3000;   // ile krokow gwintujemy "w dol"
static constexpr int32_t ZAPAS_WYJAZDU    = 300;    // dodatkowy dystans na bezpieczne wyjscie gwintownika

// Predkosci (kroki/s = Hz STEP)
static constexpr float V_BAZOWANIE = 3000.0f;
static constexpr float V_SZYBKI    = 20000.0f;
static constexpr float V_GWINT     = 6000.0f;
static constexpr float V_POWROT    = 9000.0f;


// ======================================================
// BEZPIECZENSTWO / OGRANICZENIA
// ======================================================

// PCNT ma limit watchpoint ~ 32767, ustawiamy zapas (zeby nie dobic do granicy)
static constexpr uint32_t PCNT_MAX_ODCINEK_KROKI = 30000;

// Timeouty (ms) - MUST FIX (zabezpieczenie przed jazda w nieskonczonosc)
static constexpr uint32_t TIMEOUT_BAZOWANIE_MS = 30000; // bazowanie do krancowki start (dostroisz)
static constexpr uint32_t TIMEOUT_RUCH_MS      = 60000; // pojedynczy etap ruchu/odcinek (dostroisz)

// ======================================================
// DODATKOWE (opcjonalnie na przyszlosc)
// ======================================================

// Soft-limit (opcjonalnie)
// static constexpr int32_t MAKS_ZAKRES_KROKI = 200000;

// Timeouty segmentow (opcjonalnie)
// static constexpr uint32_t TIMEOUT_BAZOWANIE_MS = 8000;
// static constexpr uint32_t TIMEOUT_SEGMENT_MS   = 10000;
