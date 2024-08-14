
#include <Arduino.h>
#include <Wire.h>
#include "i2cio.hpp"

bool i2c_probe(TwoWire &w, uint8_t addr, unsigned long timeout) {
    unsigned long t = w.getTimeOut();
    w.setTimeOut(timeout);
    w.beginTransmission(addr);
    uint8_t result = w.endTransmission();
    w.setTimeOut(t);
    return (result == 0);
}

void i2c_scan(TwoWire &w) {
    uint8_t bus = (&w == &Wire) ? 0: 1;
    log_i("scanning Wire%u", bus);
    for (auto i = 0; i < 128; i++) {
        w.beginTransmission(i);
        if (w.endTransmission() == 0) {
            log_i("Wire%u dev at 0x%x", bus, i);
        }
    }
}

