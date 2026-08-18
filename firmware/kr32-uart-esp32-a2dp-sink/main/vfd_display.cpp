#include "vfd_display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "esp_timer.h"
//#include "esp_rom/ets_sys.h"
#include <cctype>
#include "vfd_display_utils.h"

static const char* TAG = "VFDisplay";

esp_err_t ret;
spi_device_handle_t spi;

VFDisplay::VFDisplay() {}

void VFDisplay::begin(uint8_t numDigits, uint8_t brightness) {
	_numDigits = numDigits;
	
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CS) | (1ULL << Reset);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(Reset, 0);
    esp_rom_delay_us(5);
    gpio_set_level(Reset, 1);

    /*spi_bus_config_t buscfg = {};
	buscfg.miso_io_num = GPIO_NUM_19;
	buscfg.mosi_io_num = MOSI;
	buscfg.sclk_io_num = SCK;
	buscfg.quadwp_io_num = -1;
	buscfg.quadhd_io_num = -1;
	buscfg.max_transfer_sz = 2;*/
	
	spi_bus_config_t buscfg = {                                         // Provide details to the SPI_bus_sturcture of pins and maximum data size
        .mosi_io_num = MOSI,
        .miso_io_num = GPIO_NUM_19,
        .sclk_io_num = SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1                                      // 4095 bytes is the max size of data that can be sent because of hardware limitations
    };

    //spi_bus_initialize(HSPI_HOST, &buscfg, 1);

    /*spi_device_interface_config_t devcfg = {};
	devcfg.clock_speed_hz = 250*1000;
	devcfg.mode = 0;
	devcfg.flags = SPI_DEVICE_BIT_LSBFIRST;
	devcfg.spics_io_num = CS;
	devcfg.queue_size = 1;
	devcfg.cs_ena_posttrans = 1;
	devcfg.flags = SPI_DEVICE_HALFDUPLEX;*/
	
	spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
		.mode = 0,
        .clock_speed_hz = 4000000,
        //.duty_cycle_pos = 128,      //50% duty cycle
        
        
        //.cs_ena_posttrans = 3,      //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .spics_io_num = -1,
		.flags = SPI_DEVICE_BIT_LSBFIRST,
		.queue_size = 3
    };

    //spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);       // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);
	
	ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);                  // Attach the Slave device to the SPI bus
    ESP_ERROR_CHECK(ret);

    setDigitCount(_numDigits);
    setBrightness(brightness);
    ESP_LOGI(TAG, "VFD init done for %d digits", _numDigits);
}

void VFDisplay::writeDisplay(uint8_t b) {
    spi_transaction_t t = {};
    t.length = 8;
	//t.length = sizeof(b) * 8;
    //t.tx_buffer = &b;  
	t.flags = SPI_TRANS_USE_TXDATA;
	t.tx_data[0] = b;
    ret = spi_device_transmit(spi, &t); 
	/*
	spi_transaction_t trans_desc = {
        // Configure the transaction_structure
        .flags = SPI_TRANS_USE_TXDATA,                                  // Set the Tx flag
        //.tx_data = {addr, data},                                        // The host will sent the address first followed by data we provided
        .tx_buffer = &b,
		.length = 16,                                                   // Length of the address + data is 16 bits
    };*/
	//ret = spi_device_polling_transmit(spi, &trans_desc);
	//ESP_LOGI(TAG, "SPI write: 0x%02X", b);
	vTaskDelay(pdMS_TO_TICKS(1));  // або більше, якщо не критично
}

void VFDisplay::putChar(unsigned char x, char chr) {
    if (x >= _numDigits || displayBuffer[x] == chr) return;

	gpio_set_level(CS, 0);
    writeDisplay(0x20 + x);
    writeDisplay(chr);
    gpio_set_level(CS, 1);
    //show();
	
	displayBuffer[x] = chr;
}

void VFDisplay::printUTF8(const char* str, int startPos) {
    int x = startPos;
    while (*str && x < _numDigits) {
        if (str[0] == '\\' && str[1] == 'x' && isxdigit(str[2]) && isxdigit(str[3])) {
            char hex[3] = {str[2], str[3], '\0'};
            uint8_t vfd_char = strtoul(hex, NULL, 16);
            putChar(x++, vfd_char);
            str += 4;
            continue;
        }
        uint32_t cp;
        size_t len = utf8_to_unicode((const uint8_t*)str, &cp);
        if (len == 0) { cp = '?'; len = 1; }
        uint8_t vfd_char = unicode_to_vfd(cp);
        putChar(x++, vfd_char);
        str += len;
    }
	show();
}

void VFDisplay::setCustomChar(uint8_t address, const uint8_t *bitmap) {
    if (address > 7) return; // Можна записати тільки в адреси 0-7

    gpio_set_level(CS, 0);
    writeDisplay(0x40 + address); // Команда запису в CGRAM

    // Згідно з даташитом, надсилаємо 5 байтів даних (по стовпцях)
    for (int i = 0; i < 5; i++) {
        writeDisplay(bitmap[i]);
    }
    gpio_set_level(CS, 1);
    esp_rom_delay_us(5); // Пауза між командами (даташит просить мінімум 2us)
}

void VFDisplay::setDigitCount(uint8_t count) {
    gpio_set_level(CS, 0);
    writeDisplay(0xe0);
    esp_rom_delay_us(5);
    writeDisplay(count - 1);
    gpio_set_level(CS, 1);
    esp_rom_delay_us(5);
}

void VFDisplay::setBrightness(uint8_t brightness) {
    gpio_set_level(CS, 0);
    writeDisplay(0xe4);
    esp_rom_delay_us(5);
    writeDisplay(brightness);
    gpio_set_level(CS, 1);
    esp_rom_delay_us(5);
}

void VFDisplay::show() {
    gpio_set_level(CS, 0);
    writeDisplay(0xe8);
    gpio_set_level(CS, 1);
}