#pragma once
#include <stdint.h>
#include "../zdarzenia.h"

class Wejscia
{
public:
  void init();
  void update(uint32_t teraz_ms);

  bool pobierzZdarzenie(Zdarzenie& out);

  // ISR musza miec dostep statyczny
  static void isrPoczatek();
  static void isrKoniec();

private:
  void wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane = 0);
  void obsluzPrzycisk(uint32_t teraz_ms);

  // Debounce przycisku
  bool btn_stan_stabilny = true;        // true=puszczony (bo PULLUP)
  bool btn_stan_ostatni = true;
  uint32_t btn_czas_zmiany_ms = 0;

  // Flagi z ISR krancowek
  static volatile bool flaga_poczatek;
  static volatile bool flaga_koniec;

  // Kolejka zdarzen (prosta)
  static constexpr uint8_t QSIZE = 16;
  Zdarzenie q[QSIZE];
  volatile uint8_t q_head = 0;
  volatile uint8_t q_tail = 0;
};
