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
    case StanProcesu::UWALNIANIE_KRANCOWKI_POCZATKU:
    case StanProcesu::BAZOWANIE_DO_POCZATKU:
    case StanProcesu::ODJAZD_OD_POCZATKU:
    case StanProcesu::SZYBKI_DO_START_GWINTU:
    case StanProcesu::GWINTOWANIE:
    case StanProcesu::POWROT_PRZEZ_OTWOR:
    case StanProcesu::SZYBKI_DO_ZERO:
    case StanProcesu::AWARYJNE_ODSUNIECIE:
      return true;
    default:
      return false;
  }
}

bool SterownikProcesu::stanDopuszczaKraPocz(StanProcesu s) const {
  switch (s) {
    case StanProcesu::UWALNIANIE_KRANCOWKI_POCZATKU:
    case StanProcesu::BAZOWANIE_DO_POCZATKU:
    case StanProcesu::ODJAZD_OD_POCZATKU:
      return true;
    default:
      return false;
  }
}

void SterownikProcesu::uruchomAwaryjneZatrzymanie(uint32_t teraz_ms) {
  bool byl_w_gwintowaniu = (stan_biezacy == StanProcesu::GWINTOWANIE);
  bool kierunek_przed_stopem = osruchu->aktualnyKierunek();

  osruchu->stopNatychmiast();

  if (byl_w_gwintowaniu) {
    int32_t odsuniecie =
      kierunek_przed_stopem ? -AWARYJNE_ODSUNIECIE_KROKI : AWARYJNE_ODSUNIECIE_KROKI;

    przejdzDo(StanProcesu::AWARYJNE_ODSUNIECIE, teraz_ms);
    osruchu->startOdcinekBezRampy(odsuniecie, AWARYJNE_ODSUNIECIE_V);
    return;
  }

  przejdzDo(StanProcesu::BLAD_ALARM_AWARYJNY, teraz_ms);
}

void SterownikProcesu::startBezpieczneBazowanie(uint32_t teraz_ms) {
  if (wejscia->alarmAwaryjnyAktywny()) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_ALARM_AWARYJNY, teraz_ms);
    return;
  }

  osruchu->applyParametry(*p);

  if (wejscia->krPoczNaruszona()) {
    przejdzDo(StanProcesu::UWALNIANIE_KRANCOWKI_POCZATKU, teraz_ms);
    osruchu->startOdcinek((int32_t)p->odjazd_od_kranc_kroki, p->v_bazowanie);
    return;
  }

  osruchu->ustawKierunek(false);
  osruchu->startPredkosc(p->v_bazowanie);
  przejdzDo(StanProcesu::BAZOWANIE_DO_POCZATKU, teraz_ms);
}

void SterownikProcesu::update(uint32_t teraz_ms) {
  uint32_t dt = teraz_ms - czas_wejscia_ms;

  if (wejscia->alarmAwaryjnyAktywny()) {
    if (!alarm_awaryjny_latch) {
      alarm_awaryjny_latch = true;
      uruchomAwaryjneZatrzymanie(teraz_ms);
    }

    if (stan_biezacy != StanProcesu::AWARYJNE_ODSUNIECIE) {
      return;
    }
  }

  // Twarde bezpieczenstwo: krancowka KONIEC zawsze zatrzymuje.
  if (wejscia->krKoniecNaruszona() && stan_biezacy != StanProcesu::BLAD_KRANCOWKA_KONIEC) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_KONIEC, teraz_ms);
    return;
  }

  // Krancowka POCZATEK jest dozwolona tylko podczas bazowania/odjazdu/uwalniania.
  if (wejscia->krPoczNaruszona() &&
      !stanDopuszczaKraPocz(stan_biezacy) &&
      stan_biezacy != StanProcesu::BLAD_KRANCOWKA_POCZATEK) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_POCZATEK, teraz_ms);
    return;
  }

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
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_POCZATEK) return;
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
    if (stan_biezacy == StanProcesu::BLAD_ALARM_AWARYJNY) return;
  }

  while (wejscia->pobierzZdarzenie(z)) {
    obsluzZdarzenie(z, teraz_ms);
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_POCZATEK) return;
    if (stan_biezacy == StanProcesu::BLAD_KRANCOWKA_KONIEC) return;
    if (stan_biezacy == StanProcesu::BLAD_TIMEOUT) return;
    if (stan_biezacy == StanProcesu::BLAD_ALARM_AWARYJNY) return;
  }
}

void SterownikProcesu::obsluzZdarzenie(const Zdarzenie& z, uint32_t teraz_ms) {
  // bezpieczeństwo: krańcówka koniec zawsze wygrywa
  if (z.typ == TypZdarzenia::KONIEC_HIT) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_KONIEC, teraz_ms);
    return;
  }
  if (z.typ == TypZdarzenia::POCZATEK_HIT && !stanDopuszczaKraPocz(stan_biezacy)) {
    osruchu->stopNatychmiast();
    przejdzDo(StanProcesu::BLAD_KRANCOWKA_POCZATEK, teraz_ms);
    return;
  }

  switch (stan_biezacy) {
    case StanProcesu::NIE_ZBAZOWANY_POSTOJ: {
      if (z.typ == TypZdarzenia::KLIK) {
        startBezpieczneBazowanie(teraz_ms);
      }
    } break;

    case StanProcesu::UWALNIANIE_KRANCOWKI_POCZATKU: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        // Po odjezdzie od poczatku od razu start normalnego bazowania.
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
    if (z.typ == TypZdarzenia::KLIK) {
        // Toggle: gwintownik się zablokował -> przejście do POWROT z aktualnego miejsca
        const int32_t pos = osruchu->pozycjaKrokiLive();
        osruchu->stopNatychmiast();

        // Cel powrotu: przejść przez start gwintu i wyjechać o zapas
        const int32_t target = (int32_t)p->poz_start_gwintu - (int32_t)p->zapas_wyjazdu;
        const int32_t delta = target - pos; // odcinek do wykonania (może być ujemny)

        przejdzDo(StanProcesu::POWROT_PRZEZ_OTWOR, teraz_ms);
        osruchu->startOdcinek(delta, p->v_powrot);
        break;
    }

    if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::POWROT_PRZEZ_OTWOR, teraz_ms);
        int32_t powrot = -(p->dlugosc_gwintu + p->zapas_wyjazdu);
        osruchu->startOdcinek(powrot, p->v_powrot);
    }
    } break;

    case StanProcesu::POWROT_PRZEZ_OTWOR: {
    if (z.typ == TypZdarzenia::KLIK) {
        // Toggle: podczas powrotu -> przejście z aktualnego miejsca z powrotem do GWINTOWANIE
        const int32_t pos = osruchu->pozycjaKrokiLive();
        osruchu->stopNatychmiast();

        // Cel gwintowania: koniec gwintu (start + dlugosc).
        // Działa również gdy dlugosc_gwintu jest ujemna.
        const int32_t target = (int32_t)p->poz_start_gwintu + (int32_t)p->dlugosc_gwintu;
        const int32_t delta = target - pos;

        przejdzDo(StanProcesu::GWINTOWANIE, teraz_ms);
        osruchu->startOdcinek(delta, p->v_gwint);
        break;
    }

    if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        przejdzDo(StanProcesu::SZYBKI_DO_ZERO, teraz_ms);
        osruchu->startDoPozycji(POZ_ZERO, p->v_szybki);
    }
    } break;

    case StanProcesu::SZYBKI_DO_ZERO: {
    if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        // Zmiana: po cyklu wracamy do zera i wykonujemy jeszcze ponowne bazowanie
        startBezpieczneBazowanie(teraz_ms);
    }
    } break;

    case StanProcesu::AWARYJNE_ODSUNIECIE: {
      if (z.typ == TypZdarzenia::ODCINEK_DONE) {
        osruchu->stopNatychmiast();
        przejdzDo(StanProcesu::BLAD_ALARM_AWARYJNY, teraz_ms);
      }
    } break;

    case StanProcesu::BLAD_KRANCOWKA_POCZATEK:
    case StanProcesu::BLAD_KRANCOWKA_KONIEC:
    case StanProcesu::BLAD_TIMEOUT:
    case StanProcesu::BLAD_ALARM_AWARYJNY: {
      if (z.typ == TypZdarzenia::KLIK) {
        if (wejscia->alarmAwaryjnyAktywny()) {
          osruchu->stopNatychmiast();
          przejdzDo(StanProcesu::BLAD_ALARM_AWARYJNY, teraz_ms);
          return;
        }

        alarm_awaryjny_latch = false;
        // Restart przez bezpieczna procedure (z ewentualnym uwolnieniem krańcówki POCZATEK).
        startBezpieczneBazowanie(teraz_ms);
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
  } else if (nowy == StanProcesu::BLAD_KRANCOWKA_POCZATEK) {
    Serial.println("BLAD: KRANCOWKA_POCZATEK poza bazowaniem. STOP. Klik = uwolnienie+bazowanie.");
  } else if (nowy == StanProcesu::BLAD_TIMEOUT) {
    Serial.println("BLAD: TIMEOUT. STOP. Klik = bazowanie.");
  } else if (nowy == StanProcesu::BLAD_ALARM_AWARYJNY) {
    Serial.println("BLAD: ALARM_AWARYJNY. Zwolnij grzybek i kliknij aby bazowac.");
  }
}
