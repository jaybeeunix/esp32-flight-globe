#pragma once
#include <Arduino.h>
#include <Wire.h>

struct TouchPoint {
    int16_t x;
    int16_t y;
    uint8_t pressure;
};

class TouchCST328 {
public:
    TouchCST328();
    bool begin(int sda = 1, int scl = 3, int rst = 2, int irq = 4);
    bool readTouch(TouchPoint& pt);
    bool isTouched() const { return _touched; }

private:
    int _sda, _scl, _rst, _irq;
    bool _touched;
    bool i2cWrite(uint16_t reg, const uint8_t* data, size_t len);
    bool i2cRead(uint16_t reg, uint8_t* data, size_t len);
    void reset();
};

extern TouchCST328 touchController;
