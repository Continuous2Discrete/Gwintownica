#include "Wejscia.h"
#include "../config.h"
#include <Arduino.h>

volatile bool Wejscia::flaga_poczatek = false;
volatile bool Wejscia::flaga_koniec   = false;

void Wejscia::init()
{
  pinMode(PIN_BTN, INPUT_PULLUP);

  pinMode(PIN_KR_POCZ, INPUT); // jesli masz dlugie kable: rozważ zew. pulldown
  pinMode(PIN_KR_KON,  INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_KR_POCZ), Wejscia::isrPoczatek, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_KR_KON),  Wejscia::isrKoniec,   RISING);

  // stan poczatkowy przycisku (PULLUP)
  btn_stan_stabilny = (digitalRead(PIN_BTN) == HIGH);
  btn_stan_ostatni  = btn_stan_stabilny;
  btn_czas_zmiany_ms = millis();
}

void Wejscia::isrPoczatek()
{
  flaga_poczatek = true;
}

void Wejscia::isrKoniec()
{
  flaga_koniec = true;
}

void Wejscia::update(uint32_t teraz_ms)
{
  // Najpierw obsluga flag ISR (krancowki)
  if (flaga_koniec)
  {
    flaga_koniec = false;
    wrzucZdarzenie(TypZdarzenia::KONIEC_HIT, teraz_ms);
  }

  if (flaga_poczatek)
  {
    flaga_poczatek = false;
    wrzucZdarzenie(TypZdarzenia::POCZATEK_HIT, teraz_ms);
  }

  // Przycisk (debounce + klik)
  obsluzPrzycisk(teraz_ms);
}

void Wejscia::obsluzPrzycisk(uint32_t teraz_ms)
{
  bool odczyt = (digitalRead(PIN_BTN) == HIGH); // HIGH=puszczony, LOW=wcisniety

  if (odczyt != btn_stan_ostatni)
  {
    btn_stan_ostatni = odczyt;
    btn_czas_zmiany_ms = teraz_ms;
  }

  if ((teraz_ms - btn_czas_zmiany_ms) >= DEBOUNCE_BTN_MS)
  {
    if (btn_stan_stabilny != btn_stan_ostatni)
    {
      btn_stan_stabilny = btn_stan_ostatni;

      // Klik rozumiemy jako zbocze: puszczony -> wcisniety
      if (btn_stan_stabilny == false)
      {
        wrzucZdarzenie(TypZdarzenia::KLIK, teraz_ms);
      }
    }
  }
}

void Wejscia::wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane)
{
  uint8_t next = (uint8_t)((q_head + 1) % QSIZE);
  if (next == q_tail)
  {
    // kolejka pelna -> gubimy zdarzenie (na start OK)
    return;
  }
  q[q_head] = { t, dane, czas_ms };
  q_head = next;
}

bool Wejscia::pobierzZdarzenie(Zdarzenie& out)
{
  if (q_tail == q_head) return false;
  out = q[q_tail];
  q_tail = (uint8_t)((q_tail + 1) % QSIZE);
  return true;
}
