#pragma once

#include "driver/i2c.h"
#include "esp_err.h"

// Адреса PT2258 за замовчуванням (згідно даташиту)
#define PT2258_I2C_ADDR 0x44 // 7-бітна адреса 88H

class PT2258 {
private:
    i2c_port_t _i2c_port; // Номер порту (I2C_NUM_0 або I2C_NUM_1)
    uint8_t _address;     // Адреса пристрою на шині

    // Приватна функція для відправки одного байта (команди)
    esp_err_t writeByte(uint8_t data);

public:
    // Конструктор: приймає порт і адресу (якщо вона нестандартна)
    PT2258(i2c_port_t port, uint8_t addr = PT2258_I2C_ADDR);

    // Ініціалізація мікросхеми (очистка регістрів, Mute OFF)
    esp_err_t init();

    // Встановити гучність (0...79 дБ, де 0 - макс, 79 - мін)
    // channel - номер каналу (0 - усі)
    esp_err_t setVolume(uint8_t volumeDB, uint8_t channel = 0);

    // Увімкнути/вимкнути Mute
    esp_err_t mute(bool enable);
};