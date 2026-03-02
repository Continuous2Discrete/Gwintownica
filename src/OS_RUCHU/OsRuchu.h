#pragma once

#include <Arduino.h>
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"

#include "../zdarzenia.h"
#include "../Wifi_Mqtt_OTA/Parametry.h"  // <- Parametry z komponentu (dostosuj ścieżkę)

class OsRuchu {
public:
  void init();
  void update(uint32_t teraz_ms);     // obrabia flagi ISR + eventy
  void tickRampa(uint32_t dt_ms);     // wywoływać co 1 ms

  // NOWE: zastosuj aktualne parametry (cache lokalny)
  void applyParametry(const Parametry& p);

  void ustawKierunek(bool do_przodu);

  // tryb prosty (homing)
  void startPredkosc(float v_kroki_s);

  // ruch odcinkiem z rampą (start/stop 0)
  void startOdcinek(int32_t odcinek_kroki, float v_max_kroki_s);
  // ruch odcinkiem bez rampy (stala predkosc)
  void startOdcinekBezRampy(int32_t odcinek_kroki, float v_kroki_s);

  // ruch do pozycji absolutnej (w krokach), z rampą
  void startDoPozycji(int32_t poz_docelowa_kroki, float v_max_kroki_s);

  void stopNatychmiast();

  // Pozycja absolutna "na żywo" (uwzględnia postęp aktualnego odcinka/segmentu)
  // Przydaje się m.in. do "toggle" w trakcie ruchu.
  int32_t pozycjaKrokiLive() const;

  bool pobierzZdarzenie(Zdarzenie& out);

  bool jedzie() const { return czy_jedzie; }
  bool aktualnyKierunek() const { return kierunek_do_przodu; }
  float aktualnaPredkosc() const { return v_aktualna; }
  int32_t pozycjaKroki() const { return poz_kroki; }
  void ustawPozycjeKroki(int32_t p) { poz_kroki = p; }

private:
  // LEDC
  uint8_t dobierzRozdzielczosc(uint32_t freq) const;
  uint32_t duty50(uint8_t rozdz_bits) const;
  void konfigurujTimer(uint32_t freq_hz, uint8_t rozdz_bits);
  void ustawCzestotliwosc(uint32_t freq_hz);
  void ustawDuty(uint32_t duty);
  void startPwmHz(uint32_t f_hz);

  // PCNT
  void initPcnt();
  void pcntResetOdcinka();
  void pcntStartOdcinka();
  void pcntStopOdcinka();
  void pcntUstawLimitOdcinka(uint32_t kroki_abs);
  uint32_t pcntPobierzLicznikAbs() const;
  void startNastepnyKawal();

  static bool IRAM_ATTR pcnt_on_reach(
    pcnt_unit_handle_t unit,
    const pcnt_watch_event_data_t* edata,
    void* user_ctx
  );
  void onOdcinekDoneISR(uint32_t kroki_abs);

  // Event queue
  void wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane = 0);
  static constexpr uint8_t QSIZE = 8;
  Zdarzenie q[QSIZE];
  volatile uint8_t q_head = 0;
  volatile uint8_t q_tail = 0;

  // Runtime
  bool czy_jedzie = false;
  bool kierunek_do_przodu = true;
  uint32_t freq_aktualna = 0;
  uint8_t rozdz_aktualna = 0;

  // Segment / rampa
  bool tryb_odcinka = false;
  uint32_t odcinek_limit = 0;

  float v_aktualna = 0.0f;  // kroki/s
  float v_max = 0.0f;       // kroki/s
  float a = 25000.0f;       // kroki/s^2  <- będzie nadpisywane z Parametry
  bool rampa_aktywna = false;

  int32_t poz_kroki = 0;

  // ISR -> update()
  volatile bool flaga_odcinek_done = false;
  volatile uint32_t isr_odcinek_kroki = 0;

  // Długie odcinki (segmentacja)
  uint32_t odcinek_suma = 0;
  uint32_t odcinek_pozostalo = 0;
  uint32_t odcinek_zrobione = 0;
  float odcinek_vmax = 0.0f;
  bool odcinek_bez_rampy = false;

  pcnt_unit_handle_t pcnt_unit = nullptr;
  pcnt_channel_handle_t pcnt_chan = nullptr;
};
