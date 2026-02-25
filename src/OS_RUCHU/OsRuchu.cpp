#include "OsRuchu.h"
#include "../config.h"

#include <cmath>
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"

// ========================= LEDC helpers =========================
uint8_t OsRuchu::dobierzRozdzielczosc(uint32_t freq) const {
  if (freq >= 25000) return 1;
  if (freq >= 10000) return 2;
  if (freq >= 5000)  return 3;
  if (freq >= 2000)  return 4;
  if (freq >= 1024)  return 5;
  if (freq >= 975)   return 8;
  return 10;
}

uint32_t OsRuchu::duty50(uint8_t rozdz_bits) const {
  if (rozdz_bits == 0) return 0;
  return (1u << (rozdz_bits - 1));
}

void OsRuchu::konfigurujTimer(uint32_t freq_hz, uint8_t rozdz_bits) {
  ledc_timer_config_t timer_conf = {};
  timer_conf.speed_mode      = LEDC_MODE;
  timer_conf.timer_num       = LEDC_TIMER;
  timer_conf.duty_resolution = (ledc_timer_bit_t)rozdz_bits;
  timer_conf.freq_hz         = freq_hz;
  timer_conf.clk_cfg         = LEDC_AUTO_CLK;

  ledc_timer_config(&timer_conf);
  rozdz_aktualna = rozdz_bits;
  freq_aktualna  = freq_hz;
}

void OsRuchu::ustawCzestotliwosc(uint32_t freq_hz) {
  ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);
  freq_aktualna = freq_hz;
}

void OsRuchu::ustawDuty(uint32_t duty) {
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void OsRuchu::startPwmHz(uint32_t f_hz) {
  if (f_hz < MIN_FREQ_STEP) f_hz = MIN_FREQ_STEP;
  if (f_hz > MAX_FREQ_STEP) f_hz = MAX_FREQ_STEP;

  uint8_t rozdz = dobierzRozdzielczosc(f_hz);
  if (rozdz != rozdz_aktualna || freq_aktualna == 0) konfigurujTimer(f_hz, rozdz);
  else ustawCzestotliwosc(f_hz);

  ustawDuty(duty50(rozdz_aktualna));
  czy_jedzie = true;
}

// ========================= Event queue =========================
void OsRuchu::wrzucZdarzenie(TypZdarzenia t, uint32_t czas_ms, int32_t dane) {
  uint8_t next = (uint8_t)((q_head + 1) % QSIZE);
  if (next == q_tail) return;
  q[q_head] = { t, dane, czas_ms };
  q_head = next;
}

bool OsRuchu::pobierzZdarzenie(Zdarzenie& out) {
  if (q_tail == q_head) return false;
  out = q[q_tail];
  q_tail = (uint8_t)((q_tail + 1) % QSIZE);
  return true;
}

// ========================= PCNT callback =========================
bool IRAM_ATTR OsRuchu::pcnt_on_reach(
  pcnt_unit_handle_t,
  const pcnt_watch_event_data_t* edata,
  void* user_ctx
){
  auto* self = reinterpret_cast<OsRuchu*>(user_ctx);

  uint32_t kroki_abs = 0;
  if (edata) kroki_abs = (uint32_t)edata->watch_point_value;
  if (kroki_abs == 0) kroki_abs = self->odcinek_limit;

  self->onOdcinekDoneISR(kroki_abs);
  return true;
}

void OsRuchu::onOdcinekDoneISR(uint32_t kroki_abs) {
  ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
  czy_jedzie = false;
  isr_odcinek_kroki = kroki_abs;
  flaga_odcinek_done = true;
}

// ========================= PCNT init/control =========================
void OsRuchu::initPcnt() {
  pcnt_unit_config_t unit_config = {};
  unit_config.high_limit = 32767;
  unit_config.low_limit  = -32768;
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

  pcnt_chan_config_t chan_config = {};
  chan_config.edge_gpio_num  = (gpio_num_t)PIN_STEP;
  chan_config.level_gpio_num = -1;
  ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));

  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
    pcnt_chan,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE,
    PCNT_CHANNEL_EDGE_ACTION_HOLD
  ));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(
    pcnt_chan,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
    PCNT_CHANNEL_LEVEL_ACTION_KEEP
  ));

  pcnt_event_callbacks_t cbs = {};
  cbs.on_reach = &OsRuchu::pcnt_on_reach;
  ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, this));

  ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
  pcntResetOdcinka();
  pcnt_unit_stop(pcnt_unit);
}

void OsRuchu::pcntResetOdcinka() {
  if (!pcnt_unit) return;
  ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
}

void OsRuchu::pcntUstawLimitOdcinka(uint32_t kroki_abs) {
  if (!pcnt_unit) return;

  if (odcinek_limit != 0) {
    pcnt_unit_remove_watch_point(pcnt_unit, (int)odcinek_limit);
  }
  odcinek_limit = kroki_abs;
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, (int)odcinek_limit));
}

void OsRuchu::pcntStartOdcinka() {
  if (!pcnt_unit) return;
  pcntResetOdcinka();
  ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

void OsRuchu::pcntStopOdcinka() {
  if (!pcnt_unit) return;
  ESP_ERROR_CHECK(pcnt_unit_stop(pcnt_unit));
}

uint32_t OsRuchu::pcntPobierzLicznikAbs() const {
  if (!pcnt_unit) return 0;
  int count = 0;
  pcnt_unit_get_count(pcnt_unit, &count);
  if (count < 0) count = -count;
  return (uint32_t)count;
}

// ========================= Segmentacja dlugich odcinkow =========================
void OsRuchu::startNastepnyKawal() {
  if (odcinek_pozostalo == 0) return;

  uint32_t kawal = odcinek_pozostalo;
  if (kawal > PCNT_MAX_ODCINEK_KROKI) kawal = PCNT_MAX_ODCINEK_KROKI;
  odcinek_pozostalo -= kawal;

  flaga_odcinek_done = false;
  isr_odcinek_kroki = 0;

  pcntUstawLimitOdcinka(kawal);
  pcntStartOdcinka();

  v_aktualna = 0.0f;
  v_max = odcinek_vmax;

  if (v_max < (float)MIN_FREQ_STEP) v_max = (float)MIN_FREQ_STEP;
  if (v_max > (float)MAX_FREQ_STEP) v_max = (float)MAX_FREQ_STEP;

  rampa_aktywna = true;
  tryb_odcinka = true;

  startPwmHz(MIN_FREQ_STEP);
}

// ========================= NOWE: Parametry =========================
void OsRuchu::applyParametry(const Parametry& p) {
  // Minimalnie: tylko to co OsRuchu potrzebuje do rampy
  a = p.przyspieszenie_a;
}

// ========================= Public =========================
void OsRuchu::init() {
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);

  uint32_t freq_start = 1000;
  uint8_t rozdz_start = dobierzRozdzielczosc(freq_start);
  konfigurujTimer(freq_start, rozdz_start);

  ledc_channel_config_t ch_conf = {};
  ch_conf.gpio_num   = (gpio_num_t)PIN_STEP;
  ch_conf.speed_mode = LEDC_MODE;
  ch_conf.channel    = LEDC_CHANNEL;
  ch_conf.intr_type  = LEDC_INTR_DISABLE;
  ch_conf.timer_sel  = LEDC_TIMER;
  ch_conf.duty       = 0;
  ch_conf.hpoint     = 0;

  ledc_channel_config(&ch_conf);
  ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
  ustawDuty(0);

  czy_jedzie = false;
  freq_aktualna = 0;

  initPcnt();
}

void OsRuchu::update(uint32_t teraz_ms) {
  if (!flaga_odcinek_done) return;

  flaga_odcinek_done = false;

  if (pcnt_unit) pcnt_unit_stop(pcnt_unit);

  uint32_t kroki_abs = isr_odcinek_kroki;
  if (kroki_abs == 0) kroki_abs = odcinek_limit;

  int32_t delta = (int32_t)kroki_abs;
  poz_kroki += kierunek_do_przodu ? delta : -delta;

  if (tryb_odcinka) odcinek_zrobione += kroki_abs;

  pcntResetOdcinka();

  if (tryb_odcinka && odcinek_pozostalo > 0) {
    startNastepnyKawal();
    return;
  }

  rampa_aktywna = false;
  v_aktualna = 0.0f;
  v_max = 0.0f;
  tryb_odcinka = false;

  uint32_t done = odcinek_zrobione;
  if (done == 0) done = kroki_abs;

  odcinek_suma = 0;
  odcinek_pozostalo = 0;
  odcinek_zrobione = 0;
  odcinek_vmax = 0.0f;

  wrzucZdarzenie(TypZdarzenia::ODCINEK_DONE, teraz_ms, (int32_t)done);
}

void OsRuchu::tickRampa(uint32_t dt_ms) {
  if (!czy_jedzie) return;
  if (!tryb_odcinka) return;
  if (!rampa_aktywna) return;

  uint32_t zrobione = pcntPobierzLicznikAbs();
  if (zrobione >= odcinek_limit) return;

  float pozostale = (float)(odcinek_limit - zrobione);
  float s_stop = (v_aktualna * v_aktualna) / (2.0f * a);

  float dt = (float)dt_ms / 1000.0f;
  if (pozostale <= s_stop) {
    v_aktualna -= a * dt;
    if (v_aktualna < 0.0f) v_aktualna = 0.0f;
  } else {
    v_aktualna += a * dt;
    if (v_aktualna > v_max) v_aktualna = v_max;
  }

  uint32_t f = (uint32_t)(v_aktualna);
  if (f < MIN_FREQ_STEP) f = MIN_FREQ_STEP;
  if (f > MAX_FREQ_STEP) f = MAX_FREQ_STEP;

  uint8_t rozdz = dobierzRozdzielczosc(f);
  if (rozdz != rozdz_aktualna) {
    konfigurujTimer(f, rozdz);
    ustawDuty(duty50(rozdz_aktualna));
  } else {
    ustawCzestotliwosc(f);
  }
}

void OsRuchu::ustawKierunek(bool do_przodu) {
  kierunek_do_przodu = do_przodu;
  digitalWrite(PIN_DIR, do_przodu ? HIGH : LOW);
}

void OsRuchu::startPredkosc(float v_kroki_s) {
  tryb_odcinka = false;
  rampa_aktywna = false;

  if (v_kroki_s <= 0.0f) {
    stopNatychmiast();
    return;
  }

  uint32_t f = (uint32_t)v_kroki_s;
  startPwmHz(f);
}

void OsRuchu::startOdcinek(int32_t odcinek_kroki, float v_max_kroki_s) {
  if (odcinek_kroki == 0) {
    stopNatychmiast();
    wrzucZdarzenie(TypZdarzenia::ODCINEK_DONE, millis(), 0);
    return;
  }

  bool do_przodu = (odcinek_kroki > 0);
  ustawKierunek(do_przodu);

  uint32_t kroki_abs = (uint32_t)abs(odcinek_kroki);

  odcinek_suma = kroki_abs;
  odcinek_pozostalo = kroki_abs;
  odcinek_zrobione = 0;

  odcinek_vmax = v_max_kroki_s;
  if (odcinek_vmax < (float)MIN_FREQ_STEP) odcinek_vmax = (float)MIN_FREQ_STEP;
  if (odcinek_vmax > (float)MAX_FREQ_STEP) odcinek_vmax = (float)MAX_FREQ_STEP;

  startNastepnyKawal();
}

void OsRuchu::startDoPozycji(int32_t poz_docelowa_kroki, float v_max_kroki_s) {
  int32_t delta = poz_docelowa_kroki - poz_kroki;
  startOdcinek(delta, v_max_kroki_s);
}

void OsRuchu::stopNatychmiast() {
  ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
  ustawDuty(0);

  czy_jedzie = false;
  freq_aktualna = 0;

  rampa_aktywna = false;
  v_aktualna = 0.0f;
  v_max = 0.0f;
  tryb_odcinka = false;

  odcinek_suma = 0;
  odcinek_pozostalo = 0;
  odcinek_zrobione = 0;
  odcinek_vmax = 0.0f;

  pcntStopOdcinka();
}