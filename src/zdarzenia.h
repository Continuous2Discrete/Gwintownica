#pragma once
#include <stdint.h>

enum class TypZdarzenia : uint8_t
{
  BRAK = 0,
  KLIK,
  POCZATEK_HIT,
  KONIEC_HIT,
  ODCINEK_DONE
};

struct Zdarzenie
{
  TypZdarzenia typ = TypZdarzenia::BRAK;
  int32_t dane = 0;          // opcjonalne
  uint32_t czas_ms = 0;
};
