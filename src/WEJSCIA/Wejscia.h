#pragma once
#include <stdint.h>
#include "../zdarzenia.h"

class Wejscia
{
public:
  void init();
  void update(uint32_t teraz_ms);

  bool pobierzZdarzenie(Zdarzenie& out);
  bool krPoczNaruszona() const { return krp_stan_stabilny; }
  bool krKoniecNaruszona() const { return krk_stan_stabilny; }

private:
  void wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane = 0);

  void obsluzPrzycisk(uint32_t teraz_ms);
  void obsluzKrancowki(uint32_t teraz_ms);

  // Debounce przycisku
  bool btn_stan_stabilny = true;  // true=puszczony (bo PULLUP)
  bool btn_stan_ostatni  = true;
  uint32_t btn_czas_zmiany_ms = 0;

  // Debounce krancowki POCZATEK (logika: normalnie LOW, naruszenie = HIGH)
  bool krp_stan_stabilny = false;
  bool krp_stan_ostatni  = false;
  uint32_t krp_czas_zmiany_ms = 0;

  // Debounce krancowki KONIEC (logika: normalnie LOW, naruszenie = HIGH)
  bool krk_stan_stabilny = false;
  bool krk_stan_ostatni  = false;
  uint32_t krk_czas_zmiany_ms = 0;

  // Kolejka zdarzen (prosta)
  static constexpr uint8_t QSIZE = 16;
  Zdarzenie q[QSIZE];
  volatile uint8_t q_head = 0;
  volatile uint8_t q_tail = 0;
};
