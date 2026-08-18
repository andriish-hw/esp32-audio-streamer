#pragma once

#include "driver/gpio.h"
#include <stdint.h>
#include <vector>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t unicode_to_vfd(uint32_t cp);
size_t utf8_to_unicode(const uint8_t *input, uint32_t *codepoint);

#ifdef __cplusplus
}
#endif

class VFDisplay {
  public:
    VFDisplay();
	void setBrightness(uint8_t brightness);
    void begin(uint8_t numDigits = 8, uint8_t brightness = 255);
    void putChar(unsigned char x, char chr);
	void setCustomChar(uint8_t address, const uint8_t *bitmap);
	void show();
    void printUTF8(const char* str, int startPos = 0);

  private:
    void setDigitCount(uint8_t count);
    //void show();
    void writeDisplay(uint8_t b);
	uint8_t _numDigits = 8;
	char displayBuffer[32] = {0};

	gpio_num_t Reset = GPIO_NUM_15;
    gpio_num_t CS   = GPIO_NUM_12;
	gpio_num_t SCK  = GPIO_NUM_4;
	gpio_num_t MOSI = GPIO_NUM_2;
};