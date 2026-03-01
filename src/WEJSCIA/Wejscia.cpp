#include "Wejscia.h"
#include "../config.h"
#include <Arduino.h>

void Wejscia::init()
{
  // Przycisk
  pinMode(PIN_BTN, INPUT_PULLUP);

  // Krancowki
  // Zostawiam INPUT_PULLUP jak w Twoim kodzie (nie ruszam sprzętowej logiki),
  // a stabilność załatwiamy debounce'm.
  // Jeśli masz na module własne podciąganie/pull-down, INPUT też będzie OK,
  // ale trzymamy się minimalnej zmiany.
  pinMode(PIN_KR_POCZ, INPUT_PULLUP);
  pinMode(PIN_KR_KON,  INPUT_PULLUP);

  // stan poczatkowy przycisku (PULLUP): HIGH=puszczony
  btn_stan_stabilny = (digitalRead(PIN_BTN) == HIGH);
  btn_stan_ostatni  = btn_stan_stabilny;
  btn_czas_zmiany_ms = millis();

  // stan poczatkowy krancowek: interesuje nas stan logiczny "aktywny = HIGH"
  krp_stan_stabilny = (digitalRead(PIN_KR_POCZ) == HIGH);
  krp_stan_ostatni  = krp_stan_stabilny;
  krp_czas_zmiany_ms = millis();

  krk_stan_stabilny = (digitalRead(PIN_KR_KON) == HIGH);
  krk_stan_ostatni  = krk_stan_stabilny;
  krk_czas_zmiany_ms = millis();
}

void Wejscia::update(uint32_t teraz_ms)
{
  // Krancowki (debounce, zbocze LOW->HIGH generuje zdarzenie)
  obsluzKrancowki(teraz_ms);

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

      // Klik: puszczony -> wcisniety (HIGH -> LOW)
      if (btn_stan_stabilny == false)
      {
        wrzucZdarzenie(TypZdarzenia::KLIK, teraz_ms);
      }
    }
  }
}

void Wejscia::obsluzKrancowki(uint32_t teraz_ms)
{
  // ===== POCZATEK =====
  bool krp_odczyt = (digitalRead(PIN_KR_POCZ) == HIGH); // HIGH = naruszenie

  if (krp_odczyt != krp_stan_ostatni)
  {
    krp_stan_ostatni = krp_odczyt;
    krp_czas_zmiany_ms = teraz_ms;
  }

  if ((teraz_ms - krp_czas_zmiany_ms) >= DEBOUNCE_KR_MS)
  {
    if (krp_stan_stabilny != krp_stan_ostatni)
    {
      bool poprzedni = krp_stan_stabilny;
      krp_stan_stabilny = krp_stan_ostatni;

      // Zdarzenie tylko na zboczu LOW -> HIGH (naruszenie)
      if (poprzedni == false && krp_stan_stabilny == true)
      {
        wrzucZdarzenie(TypZdarzenia::POCZATEK_HIT, teraz_ms);
      }
    }
  }

  // ===== KONIEC =====
  bool krk_odczyt = (digitalRead(PIN_KR_KON) == HIGH); // HIGH = naruszenie

  if (krk_odczyt != krk_stan_ostatni)
  {
    krk_stan_ostatni = krk_odczyt;
    krk_czas_zmiany_ms = teraz_ms;
  }

  if ((teraz_ms - krk_czas_zmiany_ms) >= DEBOUNCE_KR_MS)
  {
    if (krk_stan_stabilny != krk_stan_ostatni)
    {
      bool poprzedni = krk_stan_stabilny;
      krk_stan_stabilny = krk_stan_ostatni;

      // Zdarzenie tylko na zboczu LOW -> HIGH (naruszenie)
      if (poprzedni == false && krk_stan_stabilny == true)
      {
        wrzucZdarzenie(TypZdarzenia::KONIEC_HIT, teraz_ms);
      }
    }
  }
}

void Wejscia::wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane)
{
  uint8_t next = (uint8_t)((q_head + 1) % QSIZE);
  if (next == q_tail)
  {
    // kolejka pelna -> gubimy zdarzenie
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