#include "SterownikProcesu.h"

#include "../config.h"                // TIMEOUT, POZ_ZERO, itd.
#include "../WEJSCIA/Wejscia.h"
#include "../OS_RUCHU/OsRuchu.h"
#include "../Wifi_Mqtt_OTA/Parametry.h"  // <- Parametry z komponentu (dostosuj ścieżkę)

void SterownikProcesu::init(Wejscia* wej, OsRuchu* os, Parametry* param) {
  wejscia = wej;
  osruchu = os;
  p = param;

  przejdzDo(StanProcesu::NIE_ZBAZOWANY_POSTOJ, millis());
}

bool SterownikProcesu::stanJestRuchem(StanProcesu s) const {
  switch (s) {
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

void SterownikProcesu::update(uint32_t teraz_ms) {
  uint32_t dt = teraz_ms - czas_wejscia_ms;

  if (stan_biezacy == StanProcesu::BAZOWANIE_DO_POCZATKU && dt > TIMEOUT_BAZOWANIE_MS) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_TIMEOUT, teraz_ms);
    return;
  }

  if (stanJestRuchem(stan_biezacy) &&
      stan_biezacy != StanProcesu::BAZOWANIE_DO_POCZATKU &&
      dt > TIMEOUT_RUCH_MS) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_TIMEOUT, teraz_ms);
    return;
  }

  Zdarzenie z;

  while (osruchu->pobierzZdarzenie(z)) {
    obsluzZdarzenie(z, teraz_ms);
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
  }

  while (wejscia->pobierzZdarzenie(z)) {
    obsluzZdarzenie(z, teraz_ms);
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
  }
}

void SterownikProcesu::obsluzZdarzenie(const Zdarzenie& z, uint32_t teraz_ms) {
  // bezpieczeństwo: krańcówka koniec zawsze wygrywa
  if (z.typ == TypZdarzenia::KONIEC_HIT) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_KONIEC, teraz_ms);
    return;
  }

  switch (stan_biezacy) {
    case StanProcesu::NIE_ZBAZOWANY_POSTOJ: {
      if (z.typ == TypZdarzenia::KLIK) {
        // zastosuj aktualne parametry (cache osi)
        osruchu->applyParametry(*p);

        osruchu->ustawKierunek(false);
        osruchu->startPredkosc(p->v_bazowanie);
        przejdzDo(StanProcesu::BAZOWANIE_DO_POCZATKU, teraz_ms);
      }
    } break;

    case StanProcesu::BAZOWANIE_DO_POCZATKU: {
      if (z.typ == TypZdarzenia::POCZATEK_HIT) {
        osruchu->stopNatychmiast();
        przejdzDo(StanProcesu::ODJAZD_OD_POCZATKU, teraz_ms);

        osruchu->startOdcinek((int32_t)p->odjazd_od_kranc_kroki, p->v_bazowanie);
      }
    } break;

    case StanProcesu::ODJAZD_OD_POCZATKU: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        osruchu->ustawPozycjeKroki(0);
        przejdzDo(StanProcesu::GOTOWY_POSTOJ, teraz_ms);
      }
    } break;

    case StanProcesu::GOTOWY_POSTOJ: {
      if (z.typ == TypZdarzenia::KLIK) {
        // zastosuj parametry jeszcze raz “na start cyklu”
        osruchu->applyParametry(*p);

        przejdzDo(StanProcesu::SZYBKI_DO_START_GWINTU, teraz_ms);
        osruchu->startDoPozycji(p->poz_start_gwintu, p->v_szybki);
      }
    } break;

    case StanProcesu::SZYBKI_DO_START_GWINTU: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::GWINTOWANIE, teraz_ms);
        osruchu->startOdcinek(p->dlugosc_gwintu, p->v_gwint);
      }
    } break;

    case StanProcesu::GWINTOWANIE: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::POWROT_PRZEZ_OTWOR, teraz_ms);
        int32_t powrot = -(p->dlugosc_gwintu + p->zapas_wyjazdu);
        osruchu->startOdcinek(powrot, p->v_powrot);
      }
    } break;

    case StanProcesu::POWROT_PRZEZ_OTWOR: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::SZYBKI_DO_ZERO, teraz_ms);
        osruchu->startDoPozycji(POZ_ZERO, p->v_szybki);
      }
    } break;

    case StanProcesu::SZYBKI_DO_ZERO: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::GOTOWY_POSTOJ, teraz_ms);
      }
    } break;

    case StanProcesu::BLAD_KRANCOWKA_KONIEC:
    case StanProcesu::BLAD_TIMEOUT: {
      if (z.typ == TypZdarzenia::KLIK) {
        // restart bazowania
        osruchu->applyParametry(*p);

        osruchu->ustawKierunek(false);
        osruchu->startPredkosc(p->v_bazowanie);
        przejdzDo(StanProcesu::BAZOWANIE_DO_POCZATKU, teraz_ms);
      }
    } break;

    default:
      break;
  }
}

void SterownikProcesu::przejdzDo(StanProcesu nowy, uint32_t teraz_ms) {
  stan_biezacy = nowy;
  czas_wejscia_ms = teraz_ms;

  Serial.print("FSM stan -> ");
  Serial.println((int)stan_biezacy);

  if (nowy == StanProcesu::BLAD_KRANCOWKA_KONIEC) {
    Serial.println("BLAD: KRANCOWKA_KONIEC. STOP. Klik = bazowanie.");
  } else if (nowy == StanProcesu::BLAD_TIMEOUT) {
    Serial.println("BLAD: TIMEOUT. STOP. Klik = bazowanie.");
  }
}