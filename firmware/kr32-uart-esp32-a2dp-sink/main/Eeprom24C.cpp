#include "Eeprom24C.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "Eeprom24C";

// --- Конструктор ---
Eeprom24C::Eeprom24C(i2c_port_t port, uint8_t addr) {
    _port = port;
    _addr = addr;
    _saveTimer = NULL;
    _dataDirty = false;
	memset(&_lastSavedSettings, 0, sizeof(SystemSettings));
}

// --- Callback таймера (Магія C++ та FreeRTOS) ---
// FreeRTOS не вміє викликати методи класу напряму, тільки звичайні C-функції.
// Тому ми робимо цю функцію static і отримуємо вказівник на об'єкт через ID таймера.
void Eeprom24C::_timerCallback(TimerHandle_t xTimer) {
    // Отримуємо вказівник на наш об'єкт (this), який ми зберегли при створенні таймера
    Eeprom24C* self = (Eeprom24C*) pvTimerGetTimerID(xTimer);
    
    if (self != nullptr) {
        ESP_LOGI(TAG, "Timer expired. Saving to EEPROM...");
        self->_writeToEEPROM();
    }
}

// --- Ініціалізація ---
void Eeprom24C::begin() {
    // Створюємо таймер, але не запускаємо його.
    // "SaveTimer" - ім'я
    // pdMS_TO_TICKS(10000) - час за замовчуванням (змінимо при виклику)
    // pdFALSE - One-shot (спрацювати один раз і зупинитись)
    // this - передаємо вказівник на себе, щоб callback знав, чий це таймер
    // _timerCallback - функція, яку викликати
    _saveTimer = xTimerCreate("SaveTimer", pdMS_TO_TICKS(10000), pdFALSE, (void*)this, _timerCallback);
}

// --- Розумне оновлення ---
void Eeprom24C::update(const SystemSettings &settings, uint32_t delay_ms) {
    // 1. Порівнюємо нові дані з тими, що ВЖЕ в чіпі (RAM vs RAM = миттєво)
    if (memcmp(&settings, &_lastSavedSettings, sizeof(SystemSettings)) == 0) {
        // Дані збігаються з збереженими!
        // Якщо таймер тікав - скасовуємо його, бо зберігати нічого.
        if (_dataDirty) {
            ESP_LOGI(TAG, "Change reverted. Write cancelled.");
            if (_saveTimer != NULL) xTimerStop(_saveTimer, 0);
            _dataDirty = false;
        }
        return; // Виходимо, нічого робити не треба
    }

    // 2. Якщо дані відрізняються
    _pendingSettings = settings;
    _dataDirty = true;

    // Перезапускаємо таймер
    if (_saveTimer != NULL) {
        xTimerChangePeriod(_saveTimer, pdMS_TO_TICKS(delay_ms), 0);
    }
}

// --- Фізичний запис (Приватний) ---
void Eeprom24C::_writeToEEPROM() {
    if (!_dataDirty) return;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    
    // Адреса пристрою 0x50 + Write
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    
    // M24C16 хоче ТІЛЬКИ ОДИН байт адреси комірки (word address)
    // Пишемо в комірку 0x00
    i2c_master_write_byte(cmd, 0x00, true); 
    // Другий байт (MSB) відправляти НЕ МОЖНА!
    
    // Пишемо структуру
    i2c_master_write(cmd, (uint8_t*)&_pendingSettings, sizeof(SystemSettings), true);
    
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Saved successfully!");
		// Оновлюємо еталон
        _lastSavedSettings = _pendingSettings;
        _dataDirty = false;
        // Пауза для запису самого чіпа (Write Cycle)
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    } else {
        ESP_LOGE(TAG, "Write Failed!");
    }
}

// --- Читання ---
esp_err_t Eeprom24C::load(SystemSettings &settings) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Етап 1: Dummy Write (Встановлення курсору)
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    
    // Тільки 1 байт адреси комірки
    i2c_master_write_byte(cmd, 0x00, true); 
    
    // Етап 2: Читання
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
    
    if (sizeof(SystemSettings) > 1) {
        i2c_master_read(cmd, (uint8_t*)&settings, sizeof(SystemSettings) - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, ((uint8_t*)&settings) + sizeof(SystemSettings) - 1, I2C_MASTER_NACK);
    
    i2c_master_stop(cmd);
    
    // Виконання
    esp_err_t ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
	if (ret == ESP_OK) {
        // Якщо прочитали успішно - запам'ятовуємо це як "еталон"
        _lastSavedSettings = settings;
        _dataDirty = false;
    }
    return ret; 
}

void Eeprom24C::forceSave(const SystemSettings &settings) {
    // Зупинити таймер, якщо він йшов, щоб не писати двічі
    if (_saveTimer != NULL) xTimerStop(_saveTimer, 0);
    
    _pendingSettings = settings;
    _dataDirty = true;
    _writeToEEPROM();
}