#pragma once

#include <stdint.h>
#include "../zdarzenia.h"

class Wejscia;
class OsRuchu;
struct Parametry;

enum class StanProcesu : uint8_t {
  NIE_ZBAZOWANY_POSTOJ = 0,
  UWALNIANIE_KRANCOWKI_POCZATKU,
  BAZOWANIE_DO_POCZATKU,
  ODJAZD_OD_POCZATKU,
  GOTOWY_POSTOJ,
  SZYBKI_DO_START_GWINTU,
  GWINTOWANIE,
  POWROT_PRZEZ_OTWOR,
  SZYBKI_DO_ZERO,
  AWARYJNE_ODSUNIECIE,
  BLAD_KRANCOWKA_POCZATEK,
  BLAD_KRANCOWKA_KONIEC,
  BLAD_TIMEOUT,
  BLAD_ALARM_AWARYJNY
};

class SterownikProcesu {
public:
  void init(Wejscia* wej, OsRuchu* os, Parametry* param);
  void update(uint32_t teraz_ms);

  StanProcesu stan() const { return stan_biezacy; }

private:
  void przejdzDo(StanProcesu nowy, uint32_t teraz_ms);
  void obsluzZdarzenie(const Zdarzenie& z, uint32_t teraz_ms);
  bool stanJestRuchem(StanProcesu s) const;
  bool stanDopuszczaKraPocz(StanProcesu s) const;
  void startBezpieczneBazowanie(uint32_t teraz_ms);
  void uruchomAwaryjneZatrzymanie(uint32_t teraz_ms);

  Wejscia* wejscia = nullptr;
  OsRuchu* osruchu = nullptr;
  Parametry* p = nullptr;

  StanProcesu stan_biezacy = StanProcesu::NIE_ZBAZOWANY_POSTOJ;
  uint32_t czas_wejscia_ms = 0;
  bool alarm_awaryjny_latch = false;
};
