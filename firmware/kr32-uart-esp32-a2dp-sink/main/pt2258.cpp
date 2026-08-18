#include "pt2258.h"
#include "esp_log.h"

static const char *TAG = "PT2258";

static const uint8_t set_volume_mask[][2] = {
	{0b11010000, 0b11100000},	// all channels
	{0b10000000, 0b10010000},	// channel 1
	{0b01000000, 0b01010000},	// channel 2
	{0b00000000, 0b00010000},	// channel 3
	{0b00100000, 0b00110000},	// channel 4
	{0b01100000, 0b01110000},	// channel 5
	{0b10100000, 0b10110000}	// channel 6
};

PT2258::PT2258(i2c_port_t port, uint8_t addr) {
    _i2c_port = port;
    _address = addr;
}

esp_err_t PT2258::writeByte(uint8_t data) {
    // Найпростіша функція запису в ESP-IDF
    // port, address, pointer_to_data, length, timeout_ms
    return i2c_master_write_to_device(_i2c_port, _address, &data, 1, 1000 / portTICK_PERIOD_MS);
}

esp_err_t PT2258::init() {
    ESP_LOGI(TAG, "Initializing PT2258...");
    
    // Згідно даташиту, код 0xC0 очищає регістри (System Reset)
    esp_err_t err = writeByte(0xC0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Error: %s", esp_err_to_name(err));
        return err;
    }
    
    // Потрібно трохи почекати після скидання
    // vTaskDelay(20 / portTICK_PERIOD_MS); // Якщо треба

    // Розблокувати Mute (за замовчуванням він може бути увімкнений)
    //mute(false); 
    
    return ESP_OK;
}

esp_err_t PT2258::setVolume(uint8_t volumeDB, uint8_t channel = 0) {	
	if (volumeDB > 79) volumeDB = 79; 

	// За замовчуванням вважаємо, що регулюємо один канал (або всі, якщо 0)
	uint8_t channel_1 = channel;
	uint8_t channel_2 = 0; 

	// Обробка пар каналів (7: 1-2, 8: 3-4, 9: 5-6)
	if (channel >= 7 && channel <= 9) {
		channel_1 = (channel - 7) * 2 + 1; // 7->1, 8->3, 9->5
		channel_2 = channel_1 + 1;         // ->2, ->4, ->6
	}

	uint8_t tens = volumeDB / 10;
	uint8_t units = volumeDB % 10;
	
	esp_err_t err;

	// Встановлюємо гучність для першого цільового каналу
	err = writeByte(set_volume_mask[channel_1][0] | tens); 
	if (err != ESP_OK) return err;

	err = writeByte(set_volume_mask[channel_1][1] | units); 
	if (err != ESP_OK) return err;
	
	// Якщо є другий канал (режим пари) - повторюємо для нього
	if (channel_2 != 0) {
		err = writeByte(set_volume_mask[channel_2][0] | tens);
		if (err != ESP_OK) return err;

		err = writeByte(set_volume_mask[channel_2][1] | units); 
		if (err != ESP_OK) return err;
	}
	
	ESP_LOGI(TAG, "VOLUME: -%d dB (Channel: %d)", volumeDB, channel);
	
	return ESP_OK;
}

/*esp_err_t PT2258::setVolume(uint8_t volumeDB) {
    if (volumeDB > 79) volumeDB = 79; 

    uint8_t tens = volumeDB / 10;
    uint8_t units = volumeDB % 10;
	
    // Команди для всіх каналів (Master Volume) згідно таблиці кодів PT2258:
    // Одиниці: 1101 0xxx (0xD0 + units)
    // Десятки: 1100 0xxx (0xE0 + tens)
    
	esp_err_t err;
    err = writeByte(0xD0 | tens); // Встановити десятки
    if (err != ESP_OK) return err;

    err = writeByte(0xE0 | units); // Встановити одиниці
    
	ESP_LOGI(TAG, "VOLUME: -%d dB", volumeDB);
	
    return err;
}*/

esp_err_t PT2258::mute(bool enable) {
    // Згідно даташиту: F8 (Mute OFF), F9 (Mute ON)
	ESP_LOGI(TAG, "Mute: %s", (enable ? "TRUE" : "FALSE"));
    return writeByte(enable ? 0xF9 : 0xF8);
}