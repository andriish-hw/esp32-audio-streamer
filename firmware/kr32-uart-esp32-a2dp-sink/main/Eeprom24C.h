// Невелика бібліотека для роботи з I2C EEPROM 24C02-24C16
// До конкретного з 8-ми внутрішніх блоків пам'яті, на прикладі 24C16, треба звертатися, змінюючи адресу 0x50..0x57

#pragma once
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "Settings.h"

class Eeprom24C {
private:
    i2c_port_t _port;
    uint8_t _addr;
    
    // Для відкладеного запису
    TimerHandle_t _saveTimer;
    SystemSettings _pendingSettings; // Тимчасовий буфер даних для запису
	SystemSettings _lastSavedSettings; // "Тіньова копія" того, що вже записано
    bool _dataDirty; // Прапорець "чи є незбережені зміни"

    // Статична функція-callback для FreeRTOS таймера
    static void _timerCallback(TimerHandle_t xTimer);
    
    // Приватний метод фізичного запису
    void _writeToEEPROM();

public:
    // Конструктор: тільки запам'ятовує параметри
    Eeprom24C(i2c_port_t port, uint8_t addr);

    // Ініціалізація: створює таймер (викликати в app_main)
    void begin();

    // Завантажити налаштування (викликати при старті)
    esp_err_t load(SystemSettings &settings);

    // "Розумне" оновлення: скидає таймер. Запише через delay_ms
    void update(const SystemSettings &settings, uint32_t delay_ms = 10000);
    
    // Примусовий запис (якщо треба зберегти негайно)
    void forceSave(const SystemSettings &settings);
};