#pragma once

#ifdef __cplusplus

#include <Arduino.h>

// najpierw usuń definicję z core
#ifdef Serial
  #undef Serial
#endif

class TelnetSerial : public Stream {
public:
  void begin();
  void begin(unsigned long);

  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
};

extern TelnetSerial TelnetSerialPort;

// i dopiero teraz podmień
#define Serial TelnetSerialPort

#endif // __cplusplus