#pragma once
#include <Arduino.h>
#include <Wire.h>

bool i2c_probe(TwoWire &w, uint8_t addr, unsigned long timeout = 1000);
void i2c_scan(TwoWire &w);

static inline uint8_t detect(TwoWire &w, uint8_t addr, unsigned long timeout = 1000) {
    return (i2c_probe(w, addr, timeout )) ?  addr : 0;
} 