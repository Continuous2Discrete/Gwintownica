#include "SterownikProcesu.h"
#include "../config.h"
#include "../WEJSCIA/Wejscia.h"
#include "../OS_RUCHU/OsRuchu.h"
#include <Arduino.h>

void SterownikProcesu::init(Wejscia* wej, OsRuchu* os)
{
  wejscia = wej;
  osruchu = os;
  przejdzDo(StanProcesu::NIE_ZBAZOWANY_POSTOJ, millis());
}

bool SterownikProcesu::stanJestRuchem(StanProcesu s) const
{
  switch (s)
  {
    case StanProcesu::BAZOWANIE_DO_POCZATKU:
    case StanProcesu::ODJAZD_OD_POCZATKU:
    case StanProcesu::SZYBKI_DO_START_GWINTU:
    case StanProcesu::GWINTOWANIE:
    case StanProcesu::POWROT_PRZEZ_OTWOR:
    case StanProcesu::SZYBKI_DO_ZERO:
      return true;
    default:
      return false;
  }
}

void SterownikProcesu::update(uint32_t teraz_ms)
{
  // MUST FIX: timeouty
  uint32_t dt = teraz_ms - czas_wejscia_ms;

  if (stan_biezacy == StanProcesu::BAZOWANIE_DO_POCZATKU && dt > TIMEOUT_BAZOWANIE_MS)
  {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_TIMEOUT, teraz_ms);
    return;
  }

  if (stanJestRuchem(stan_biezacy) &&
      stan_biezacy != StanProcesu::BAZOWANIE_DO_POCZATKU &&
      dt > TIMEOUT_RUCH_MS)
  {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_TIMEOUT, teraz_ms);
    return;
  }

  Zdarzenie z;

  // 1) eventy z osi
  while (osruchu->pobierzZdarzenie(z))
  {
    obsluzZdarzenie(z, teraz_ms);
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
  }

  // 2) eventy z wejsc
  while (wejscia->pobierzZdarzenie(z))
  {
    obsluzZdarzenie(z, teraz_ms);
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
  }
}

void SterownikProcesu::obsluzZdarzenie(const Zdarzenie& z, uint32_t teraz_ms)
{
  // bezpieczenstwo: krancowka koniec zawsze wygrywa
  if (z.typ == TypZdarzenia::KONIEC_HIT)
  {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_KONIEC, teraz_ms);
    return;
  }

  switch (stan_biezacy)
  {
    case StanProcesu::NIE_ZBAZOWANY_POSTOJ:
    {
      if (z.typ == TypZdarzenia::KLIK)
      {
        osruchu->ustawKierunek(false);
        osruchu->startPredkosc(V_BAZOWANIE);
        przejdzDo(StanProcesu::BAZOWANIE_DO_POCZATKU, teraz_ms);
      }
    } break;

    case StanProcesu::BAZOWANIE_DO_POCZATKU:
    {
      if (z.typ == TypZdarzenia::POCZATEK_HIT)
      {
        osruchu->stopNatychmiast();
        przejdzDo(StanProcesu::ODJAZD_OD_POCZATKU, teraz_ms);
        osruchu->startOdcinek((int32_t)ODJAZD_OD_KRANCOWKI_KROKI, V_BAZOWANIE);
      }
    } break;

    case StanProcesu::ODJAZD_OD_POCZATKU:
    {
      if (z.typ == TypZdarzenia::ODCINEK_DONE)
      {
        osruchu->ustawPozycjeKroki(0);
        przejdzDo(StanProcesu::GOTOWY_POSTOJ, teraz_ms);
      }
    } break;

    case StanProcesu::GOTOWY_POSTOJ:
    {
      if (z.typ == TypZdarzenia::KLIK)
      {
        przejdzDo(StanProcesu::SZYBKI_DO_START_GWINTU, teraz_ms);
        osruchu->startDoPozycji(POZ_START_GWINTU, V_SZYBKI);
      }
    } break;

    case StanProcesu::SZYBKI_DO_START_GWINTU:
    {
      if (z.typ == TypZdarzenia::ODCINEK_DONE)
      {
        przejdzDo(StanProcesu::GWINTOWANIE, teraz_ms);
        osruchu->startOdcinek(DLUGOSC_GWINTU, V_GWINT);
      }
    } break;

    case StanProcesu::GWINTOWANIE:
    {
      if (z.typ == TypZdarzenia::ODCINEK_DONE)
      {
        przejdzDo(StanProcesu::POWROT_PRZEZ_OTWOR, teraz_ms);
        int32_t powrot = -(DLUGOSC_GWINTU + ZAPAS_WYJAZDU);
        osruchu->startOdcinek(powrot, V_POWROT);
      }
    } break;

    case StanProcesu::POWROT_PRZEZ_OTWOR:
    {
      if (z.typ == TypZdarzenia::ODCINEK_DONE)
      {
        przejdzDo(StanProcesu::SZYBKI_DO_ZERO, teraz_ms);
        osruchu->startDoPozycji(POZ_ZERO, V_SZYBKI);
      }
    } break;

    case StanProcesu::SZYBKI_DO_ZERO:
    {
      if (z.typ == TypZdarzenia::ODCINEK_DONE)
      {
        przejdzDo(StanProcesu::GOTOWY_POSTOJ, teraz_ms);
      }
    } break;

    case StanProcesu::BLAD_KRANCOWKA_KONIEC:
    case StanProcesu::BLAD_TIMEOUT:
    {
      // klik -> bazowanie od nowa
      if (z.typ == TypZdarzenia::KLIK)
      {
        osruchu->ustawKierunek(false);
        osruchu->startPredkosc(V_BAZOWANIE);
        przejdzDo(StanProcesu::BAZOWANIE_DO_POCZATKU, teraz_ms);
      }
    } break;

    default:
      break;
  }
}

void SterownikProcesu::przejdzDo(StanProcesu nowy, uint32_t teraz_ms)
{
  stan_biezacy = nowy;
  czas_wejscia_ms = teraz_ms;

  Serial.print("FSM stan -> ");
  Serial.println((int)stan_biezacy);

  if (nowy == StanProcesu::BLAD_KRANCOWKA_KONIEC)
  {
    Serial.println("BLAD: KRANCOWKA_KONIEC. STOP. Klik = bazowanie.");
  }
  else if (nowy == StanProcesu::BLAD_TIMEOUT)
  {
    Serial.println("BLAD: TIMEOUT. STOP. Klik = bazowanie.");
  }
}
