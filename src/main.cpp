#include <Arduino.h>
#include "config.h"
#include "zdarzenia.h"
#include "WEJSCIA/Wejscia.h"
#include "OS_RUCHU/OsRuchu.h"
#include "STEROWNIK_PROCESU/SterownikProcesu.h"
#include "Wifi_Mqtt_OTA/Agent.h"

Agent agent;
Wejscia wej;
OsRuchu os;
SterownikProcesu sterownikProcesu;

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("Start Gwintownica");
  agent.begin();

  wej.init();
  os.init();
  sterownikProcesu.init(&wej, &os, &agent.parametry());

  Serial.println("Gotowe: klik -> bazowanie do krancowki POCZATEK");
}

static uint32_t last_tick = 0;

void loop()
{
  uint32_t teraz = millis();

  // 1) update osi (flagi ISR -> eventy)
  os.update(teraz);

  // 2) tick rampy co 1 ms
  if ((uint32_t)(teraz - last_tick) >= TICK_RAMPA_MS)
  {
    last_tick = teraz;
    os.tickRampa(TICK_RAMPA_MS);
  }

  // 3) IO + FSM
  wej.update(teraz);
  sterownikProcesu.update(teraz);
  agent.loop();
}

