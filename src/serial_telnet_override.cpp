#include <Arduino.h>
#include <TelnetStream.h>
#include "serial_telnet_override.h"

TelnetSerial TelnetSerialPort;

void TelnetSerial::begin() { TelnetStream.begin(); }
void TelnetSerial::begin(unsigned long) { TelnetStream.begin(); }

size_t TelnetSerial::write(uint8_t c) { return TelnetStream.write(c); }
size_t TelnetSerial::write(const uint8_t* buf, size_t size) { return TelnetStream.write(buf, size); }

int TelnetSerial::available() { return TelnetStream.available(); }
int TelnetSerial::read() { return TelnetStream.read(); }
int TelnetSerial::peek() { return TelnetStream.peek(); }
void TelnetSerial::flush() { TelnetStream.flush(); }