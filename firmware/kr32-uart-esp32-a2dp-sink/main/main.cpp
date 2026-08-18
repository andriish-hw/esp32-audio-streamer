#define BT_NAME_STR "Audiostreamer"

////////// Components //////////
#define USE_I2C 1
#define USE_PT2258 1
#define USE_Eeprom24C 1

////////// PINOUT //////////
// I2S
#define LRCK 26
#define BCLK 22
#define DATA 25

// Rotary encoder
#define ENC_A GPIO_NUM_33
#define ENC_B GPIO_NUM_32

// Buttons
#define BTN_PLAY GPIO_NUM_19
#define BTN_PREV GPIO_NUM_18
#define BTN_NEXT GPIO_NUM_5
#define BTN_ENC GPIO_NUM_35

// Rotary switch
#define ROT_SW_BT_RAD GPIO_NUM_34
#define ROT_SW_A GPIO_NUM_39
#define ROT_SW_B GPIO_NUM_36

////////// Display //////////
#define DISPLAY_DIGITS_NUM 16
#define DISPLAY_CGROM_TABLE 7	// Номер таблиці символів (для залежності від конкретних символів): 
								// 2 - велика кирилиця + знаки стану відтворення
								// 7 - велика і мала кирилиця

////////// Settings //////////
// Intervals (ms)
#define SCREEN_NAME_SHOW_INT 1000
#define VOL_SCR_SHOW_INT 1500
#define MODE_SCR_SHOW_INT 800
#define DELAY_MODE_CHANGE 100

#define MAX_STEPS 25	// max volume steps

#define USE_HW_VOL_CTRL_BT 1	// use PT2258 volume control instead of software volume control
#define USE_HW_VOL_CTRL_KR 1

// Volume offsets
#define VOL_OFFSET_MODE_BT -6	// BT mode volume offset in dB

////////// UART //////////
#define UART_PORT UART_NUM_2
#define UART_TX 23
#define UART_RX 21
#define BUF_SIZE 1024
#define TAG_UART_HDLR "UART_HANDLER"

////////// I2C //////////
#define I2C_MASTER_SCL_IO           14    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           27    // GPIO number for I2C master data
#define I2C_GPIO_PULLUP 0
#define I2C_MASTER_NUM              I2C_NUM_0 
#define I2C_MASTER_FREQ_HZ          100000 // 100kHz

#define TAG_PT2258 "PT2258"
#define EEPROM_ADDR 0x50

////////// Debug //////////
#define DISP_KR_LOG_WHILE_LOADING 0

#define EXT_I2S_CTRL 0

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "BluetoothA2DPSinkQueued.h"
#include "vfd_display.h"
#include "vfd_scrolling.h"
#include "driver/uart.h"
#if USE_I2C
#include "driver/i2c.h"
#endif
#if USE_PT2258
#include "pt2258.h"
#endif
#include "Settings.h"
#if USE_Eeprom24C
#include "Eeprom24C.h"
#endif
#include "driver/pcnt.h"

I2SStream out;
#if EXT_I2S_CTRL
BluetoothA2DPSinkQueued a2dp_sink;
auto cfg = out.defaultConfig();
#else
BluetoothA2DPSinkQueued a2dp_sink(out);
//BluetoothA2DPSink a2dp_sink(out);	
#endif

#if USE_HW_VOL_CTRL_BT
A2DPNoVolumeControl no_volume_control;
#endif

VFDisplay vfdisplay;
#if USE_PT2258
PT2258 pt2258(I2C_MASTER_NUM);
#endif
SystemSettings currentSettings;
#if USE_Eeprom24C
Eeprom24C eeprom(I2C_MASTER_NUM, EEPROM_ADDR);
#endif

static TaskHandle_t main_task_handle = NULL;
volatile int last_triggered_pin = -1;

esp_avrc_playback_stat_t playback_status = ESP_AVRC_PLAYBACK_STOPPED;

extern char current_codec[16];

bool wasConnected = false;

uint32_t codec_rate = 0; 
uint8_t codec_bps = 0; 
uint8_t codec_channels = 0;
uint64_t volume_scr_start = 0;
uint64_t scr_name_start = 0;
uint64_t mode_name_start = 0;
bool need_show_title = false;
bool skip_title_scr = false;
bool need_show_scr_name = false;
uint32_t playpos = 0;

volatile int8_t enc_delta = 0;
portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// Режими інтерфейсу (для майбутніх налаштувань)
enum UI_MODE {
    UI_MODE_NORMAL,   // Звичайний режим (Гучність / Екрани)
    UI_MODE_SETTINGS, // Навігація по меню (резерв на майбутнє)
    UI_MODE_EDIT      // Редагування значення (резерв на майбутнє)
};

UI_MODE current_ui_mode = UI_MODE_NORMAL;

#define MAX_METADATA_LEN 100

char current_title[MAX_METADATA_LEN] = "";
char current_artist[MAX_METADATA_LEN] = "";
char current_album[MAX_METADATA_LEN] = "";
char display_buffer[MAX_METADATA_LEN * 2 + 4] = ""; // запас на " / " і '\0'

TaskHandle_t button_task_handle = NULL;
TaskHandle_t text_prepare_task_handle = NULL;
volatile uint8_t scrlMode = 0;
uint8_t dispScreen = 0;
uint8_t lastScreen = 0;

enum DISP_SCREENS {
  SCR_BT_RDY,
  SCR_SRC_NAME,
  SCR_TITLE_ARTIST,
  SCR_ALBUM_ARTIST,
  SCR_PLAY_POS,
  SCR_CODEC_CFG,
  SCR_DISCONNECTED,
  SCR_VOLUME,
  SCR_RADIO_TITLE,
  SCR_MODE_NAME
};
//uint8_t defScreen = SCR_TITLE_ARTIST;

// Українська 'Є'
const uint8_t char_YE[5] = {
    0x3E, // Стовпець 1: 0111110 (дуга зліва)
    0x49, // Стовпець 2: 1001001 (верх, низ, серединка)
    0x49, // Стовпець 3: 1001001
    0x49, // Стовпець 4: 1000001
    0x22  // Стовпець 5: 0100010 (краї справа)
};

// Українська 'є' (мала)
const uint8_t char_ye_small[5] = {
    0x38, // 0111000 (нижня дуга)
    0x54, // 1010100
    0x54, // 1010100
    0x44, // 1000100
    0x28  // 0101000
};

const uint8_t icon_play[5] = {
    0x00, // 0000000
    0x7F, // 1111111
    0x3E, // 0111110
    0x1C, // 0011100
    0x08  // 0001000
};

const uint8_t icon_pause[5] = {
    0x7F, // 1111111
    0x7F, // 1111111
    0x00, // 0000000
    0x7F, // 1111111
    0x7F  // 1111111
};

const uint8_t icon_stop[5] = {
    0x3E, // 0111110
    0x3E, // 0111110
    0x3E, // 0111110
    0x3E, // 0111110
    0x3E  // 0111110
};

bool ff_mode = false;
bool rew_mode = false;

typedef enum {
    MODE_BT,
    MODE_RADIO,
    MODE_IN1,
    MODE_IN2,
	MODE_IN3
} sys_mode_t;

sys_mode_t mode = MODE_BT;
static uint8_t _mode_sw_state = 255;  // 255 потрібно для того, щоб примусово викликати обробку вибору джерела і встановлення гучності

 // Масив прив'язки режимів до коду стану перемикача (стани входів, що упаковані в 3 біти ROT_SW_BT_RAD:ROT_SW_B:ROT_SW_A)
sys_mode_t sw_state_to_mode[] = {MODE_BT, MODE_IN1, MODE_IN3, MODE_IN2, MODE_RADIO};

bool radio_ready = false;
bool radio_isPlaying = false;
uint8_t radio_st_num = 0;
uint8_t radio_st_qty = 0;
char radio_name[MAX_METADATA_LEN] = "";
char radio_icy0[MAX_METADATA_LEN] = "Loading ";
char radio_icy4[MAX_METADATA_LEN] = "";
char radio_meta[MAX_METADATA_LEN * 2 + 4] = "";
char radio_title[MAX_METADATA_LEN] = "";
char radio_artist[MAX_METADATA_LEN] = "";
//#if DISP_KR_LOG_WHILE_LOADING
char radio_data[32] = "";
//#endif
bool radio_vol_synced = false;

static const uint8_t volume_127_to_pt2258[128] = {
	79, 62, 58, 55, 53, 50, 48, 47, 46, 45, 44, 43, 42, 41, 41, 40, 39, 39, 38, 37, 
	37, 36, 36, 35, 34, 34, 33, 33, 32, 32, 31, 31, 30, 30, 29, 29, 29, 28, 28, 27, 
	27, 27, 26, 26, 25, 25, 25, 24, 24, 24, 23, 23, 23, 22, 22, 22, 21, 21, 21, 21, 
	20, 20, 20, 19, 19, 19, 18, 18, 18, 18, 17, 17, 16, 16, 16, 15, 15, 15, 14, 14, 
	14, 14, 14, 13, 13, 13, 12, 12, 12, 12, 11, 11, 11, 10, 10, 10, 10, 9, 9, 9, 
	8, 8, 8, 7, 7, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0
};	// LUT для перетворення діапазону гучності 0..127 в -79..0 дБ 
	// послаблення для PT2258 з відповідністю кривій гучності

static const uint8_t volume_offsets[5] = { -VOL_OFFSET_MODE_BT, 0, 0, 0, 0 };	// Оффсет гучності для кожного режиму

#if EXT_I2S_CTRL
// Write data to I2S
void read_data_stream(const uint8_t *data, uint32_t length) {
	out.write(data, length);
}
#endif

struct Button {
    gpio_num_t pin;
    bool prev_raw = true; // рівень сигналу на попередньому циклі
    bool long_press_active = false;
    bool hold_reported = false;
    bool long_press_detected = false;
    bool click_pending = false;
    uint32_t pressed_time = 0;

    void init() {
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
        prev_raw = gpio_get_level(pin);  // щоб не було помилкового переходу
    }

    // Потрібно викликати в кожному циклі
    void update(uint32_t now_ms, uint32_t threshold_ms = 600) {
        bool current = gpio_get_level(pin); // true = not pressed
        bool pressed = !current;

        // Rising edge: відпущено
        if (prev_raw == 0 && current == 1) {
            if (!long_press_detected) {
                click_pending = true;
            }
            long_press_active = false;
            hold_reported = false;
            long_press_detected = false;
        }

        // Falling edge: натиснуто
        if (prev_raw == 1 && current == 0) {
            pressed_time = now_ms;
            long_press_active = false;
            hold_reported = false;
            long_press_detected = false;
        }

        // Утримання
        if (pressed) {
            if (!long_press_active && (now_ms - pressed_time > threshold_ms)) {
                long_press_active = true;
                long_press_detected = true;
            }
        }

        prev_raw = current;
    }

    bool is_click() {
        if (click_pending) {
            click_pending = false;
            return true;
        }
        return false;
    }

    bool is_hold() {
        if (long_press_active && !hold_reported) {
            hold_reported = true;
            return true;
        }
        return false;
    }

    bool check_long_press() {
        return long_press_active;
    }

    bool is_held() {
        return !gpio_get_level(pin);
    }
};

class HardwareRotary {
public:
    HardwareRotary(gpio_num_t pinA, gpio_num_t pinB, int16_t min_limit = -32000, int16_t max_limit = 32000) 
        : _unit(PCNT_UNIT_0) { // Використовуємо Unit 0
        
        // Налаштування каналу 0 (Pin A - Pulse, Pin B - Control)
        pcnt_config_t pcnt_confA = {
            .pulse_gpio_num = pinA,
            .ctrl_gpio_num = pinB,
            .lctrl_mode = PCNT_MODE_REVERSE, // Якщо B високий - рахуємо назад
            .hctrl_mode = PCNT_MODE_KEEP,    // Якщо B низький - рахуємо вперед
            .pos_mode = PCNT_COUNT_INC,      // Наростаючий фронт A - плюс
            .neg_mode = PCNT_COUNT_DEC,      // Спадаючий фронт A - мінус
            .counter_h_lim = max_limit,
            .counter_l_lim = min_limit,
            .unit = _unit,
            .channel = PCNT_CHANNEL_0,
        };
        
        // Налаштування каналу 1 (Pin B - Pulse, Pin A - Control) - для повної точності (4x)
        pcnt_config_t pcnt_confB = {
            .pulse_gpio_num = pinB,
            .ctrl_gpio_num = pinA,
            .lctrl_mode = PCNT_MODE_KEEP,    // Логіка інвертується для другого каналу
            .hctrl_mode = PCNT_MODE_REVERSE,
            .pos_mode = PCNT_COUNT_INC,
            .neg_mode = PCNT_COUNT_DEC,
            .counter_h_lim = max_limit,
            .counter_l_lim = min_limit,
            .unit = _unit,
            .channel = PCNT_CHANNEL_1,
        };

        // Ініціалізація
        pcnt_unit_config(&pcnt_confA);
        pcnt_unit_config(&pcnt_confB);

        // --- ВАЖЛИВО: АПАРАТНИЙ ФІЛЬТР БРЯЗКОТУ ---
        // Відфільтровує імпульси коротші за поріг (у тактах APB clock, 80MHz)
        // 1000 тактів ≈ 12.5 мікросекунд. Це вбиває брязкіт апаратно.
        pcnt_set_filter_value(_unit, 1000); 
        pcnt_filter_enable(_unit);

        // Очистка і старт
        pcnt_counter_pause(_unit);
        pcnt_counter_clear(_unit);
        pcnt_counter_resume(_unit);
    }

    // Повертає зміну позиції з моменту останнього виклику
    int16_t getDelta() {
        int16_t count = 0;
        pcnt_get_counter_value(_unit, &count);
        if (count != 0) {
            pcnt_counter_clear(_unit); // Скидаємо лічильник після читання
        }
        return count;
    }

private:
    pcnt_unit_t _unit;
};

// Створюємо глобальний об'єкт (Pin A, Pin B)
HardwareRotary encoder(ENC_A, ENC_B);

static void IRAM_ATTR rot_sw_handler(void* arg) {
	gpio_intr_disable(ROT_SW_BT_RAD);
    gpio_intr_disable(ROT_SW_A);
    gpio_intr_disable(ROT_SW_B);
    // 1. Запам'ятовуємо, який пін спрацював
    last_triggered_pin = (int)arg;
    // 2. Сповіщаємо main task, що пора прокидатися
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (main_task_handle != NULL) {
        vTaskNotifyGiveFromISR(main_task_handle, &xHigherPriorityTaskWoken);
    }

    // 3. Якщо main task має вищий пріоритет, ніж поточна задача - перемикаємось миттєво
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void gpio_config() {
	/*gpio_config_t io_conf_enc_intr = {
        .pin_bit_mask = (1ULL << ENC_A),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf_enc_intr);*/

    gpio_config_t io_conf_btn = {
        .pin_bit_mask = (1ULL << BTN_PLAY) | (1ULL << BTN_PREV) | (1ULL << BTN_NEXT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_btn);
	
	gpio_config_t io_conf_enc_btn = {
        .pin_bit_mask = (1ULL << BTN_ENC), //| (1ULL << ENC_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_enc_btn);
	
	gpio_config_t io_conf_rot_sw = {
        .pin_bit_mask = (1ULL << ROT_SW_A) | (1ULL << ROT_SW_B) | (1ULL << ROT_SW_BT_RAD),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf_rot_sw);

    gpio_install_isr_service(0); // 0 – стандартний прапор
	gpio_isr_handler_add(ROT_SW_BT_RAD, rot_sw_handler, (void*) ROT_SW_BT_RAD);
    gpio_isr_handler_add(ROT_SW_A,      rot_sw_handler, (void*) ROT_SW_A);
    gpio_isr_handler_add(ROT_SW_B,      rot_sw_handler, (void*) ROT_SW_B);
    //gpio_isr_handler_add(ENC_A, encoder_isr_handler, NULL);
}

void i2c_init(void) {
	i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    #if I2C_GPIO_PULLUP
		conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
		conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	#endif
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
	conf.clk_flags = 0;
	
	ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

void radioMetaClear(void) {
	radio_meta[0] = '\0';
	radio_icy0[0] = '\0';
	radio_icy4[0] = '\0';
	radio_artist[0] = '\0';
	radio_title[0] = '\0';
}

typedef enum {
    STATE_WAIT_CLI,
    STATE_PARSE_TYPE,
    STATE_SKIP_SPACES,
    STATE_READ_VALUE
} parser_state_t;

typedef enum {
	KR_START,
	KR_STOP,
	KR_GET_VOL,
    KR_SET_VOL,
    KR_PLAY_ST_NUM,
	KR_NEXT,
	KR_PREV,
	KR_INFO,
	KR_LIST
} radio_cmd_t;

static parser_state_t state = STATE_WAIT_CLI;
static char type_buf[16];
static size_t type_len = 0;
static char value_buf[256];
static size_t value_len = 0;

void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); 
}

void uart_cmd_send(radio_cmd_t cmd, int val = 0) {
    char cmd_out[64];

    switch (cmd) {
        case KR_START:
            snprintf(cmd_out, sizeof(cmd_out), "cli.start");
            break;
        case KR_STOP:
            snprintf(cmd_out, sizeof(cmd_out), "cli.stop");
            break;
		case KR_GET_VOL:
            snprintf(cmd_out, sizeof(cmd_out), "cli.vol");
            break;
        case KR_SET_VOL:
            snprintf(cmd_out, sizeof(cmd_out), "cli.vol(\"%d\")", val);
            break;
		case KR_PLAY_ST_NUM:
			snprintf(cmd_out, sizeof(cmd_out), "cli.play(\"%d\")", val);
            break;
		case KR_NEXT:
			snprintf(cmd_out, sizeof(cmd_out), "cli.next");
            break;
		case KR_PREV:
			snprintf(cmd_out, sizeof(cmd_out), "cli.prev");
            break;
		case KR_INFO:
			snprintf(cmd_out, sizeof(cmd_out), "cli.info");
			break;
		case KR_LIST:
			snprintf(cmd_out, sizeof(cmd_out), "cli.list");
			break;
		default:
			break;
    }

    uart_write_bytes(UART_PORT, cmd_out, strlen(cmd_out));
    uart_write_bytes(UART_PORT, "\n", 1);
}

void setSysVolume(int new_vol, uint8_t apply_to = 255, bool show_on_display = true) {
	if (new_vol > 127) new_vol = 127;
	if (new_vol < 0) new_vol = 0;
	if (new_vol != currentSettings.master_volume) {
		currentSettings.master_volume = new_vol;
		eeprom.update(currentSettings);
	}
	switch (apply_to) {
		case 0:
			a2dp_sink.set_volume(currentSettings.master_volume);
		#if USE_HW_VOL_CTRL_BT
			pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[MODE_BT], 7);
		#endif
			break;
		case 1:
			uart_cmd_send(KR_SET_VOL, (currentSettings.master_volume == 0) ? 1 : 2 * currentSettings.master_volume);
			break;
		case 2:
			pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[mode], 7);
			break;
		case 3:
			if (!USE_HW_VOL_CTRL_KR) uart_cmd_send(KR_SET_VOL, (currentSettings.master_volume == 0) ? 1 : 2 * currentSettings.master_volume);
			if ((!USE_HW_VOL_CTRL_BT && mode == MODE_BT) || (!USE_HW_VOL_CTRL_KR && mode == MODE_RADIO)) {
				pt2258.setVolume((mode == MODE_BT ? -VOL_OFFSET_MODE_BT : 0), 7);
			} else {
				//pt2258.setVolume(79 - (currentSettings.master_volume * 79 + 127 / 2) / 127);
				pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[mode], 7);
			}
			break;
		case 4:
			a2dp_sink.set_volume(currentSettings.master_volume);
			if ((!USE_HW_VOL_CTRL_BT && mode == MODE_BT) || mode == MODE_RADIO) {
				pt2258.setVolume(mode == MODE_BT ? -VOL_OFFSET_MODE_BT : 0, 7);
			} else {
				//pt2258.setVolume(79 - (currentSettings.master_volume * 79 + 127 / 2) / 127);
				pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[mode], 7);
			}
			break;
		case 5:
			a2dp_sink.set_volume(currentSettings.master_volume);
			uart_cmd_send(KR_SET_VOL, (currentSettings.master_volume == 0) ? 1 : 2 * currentSettings.master_volume);
			break;
		case 6: 
			switch (mode) {
				case MODE_BT:
				#if USE_HW_VOL_CTRL_BT
					pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[MODE_BT], 7);
				#else
					pt2258.setVolume(-VOL_OFFSET_MODE_BT, 7);
				#endif
					break;
				case MODE_RADIO: 
					if (!USE_HW_VOL_CTRL_KR) {
						uart_cmd_send(KR_SET_VOL, (currentSettings.master_volume == 0) ? 1 : 2 * currentSettings.master_volume); 
						pt2258.setVolume(0, 7);
						break;
					}
				case MODE_IN1: 
				case MODE_IN2:
				case MODE_IN3: pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume] + volume_offsets[mode], 7);
			}
			a2dp_sink.set_volume(currentSettings.master_volume);
			break;
		case 7: break;
		/*default:
			a2dp_sink.set_volume(currentSettings.master_volume);
			uart_cmd_send(KR_SET_VOL, (currentSettings.master_volume == 0) ? 1 : 2 * currentSettings.master_volume);
			//pt2258.setVolume(79 - (currentSettings.master_volume * 79 + 127 / 2) / 127);
			pt2258.setVolume(volume_127_to_pt2258[currentSettings.master_volume], 7);
			break;*/
	}
	if (USE_HW_VOL_CTRL_KR) uart_cmd_send(KR_SET_VOL, 254);
	if (show_on_display) {
		if (dispScreen != SCR_VOLUME) lastScreen = dispScreen;
		volume_scr_start = esp_timer_get_time() / 1000;
		dispScreen = SCR_VOLUME;
		xTaskNotifyGive(text_prepare_task_handle);
	}
}

void setHeadphonesVolume(int new_vol, bool sync_absolute_volume, bool show_on_display = true) {
	bool independent_hp_vol = (USE_HW_VOL_CTRL_BT && USE_HW_VOL_CTRL_KR);
	
	if (new_vol > 127) new_vol = 127;
	if (new_vol < 0) new_vol = 0;
	if (new_vol != currentSettings.headphones_volume) {
		currentSettings.headphones_volume = new_vol;
		eeprom.update(currentSettings);
	}
	if (USE_HW_VOL_CTRL_BT && sync_absolute_volume) a2dp_sink.set_volume(currentSettings.headphones_volume);
	pt2258.setVolume(volume_127_to_pt2258[currentSettings.headphones_volume] + ((independent_hp_vol) ? volume_offsets[mode] : 0), 8);
	
	if (show_on_display) {
		if (dispScreen != SCR_VOLUME) lastScreen = dispScreen;
		volume_scr_start = esp_timer_get_time() / 1000;
		dispScreen = SCR_VOLUME;
		xTaskNotifyGive(text_prepare_task_handle);
	}
}

// Універсальний декодер кодування
void decode_to_utf8(const char *src, char *dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;
    
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dest_size - 1; i++) {
        uint8_t c = (uint8_t)src[i];

        if (c < 128) { // Латиниця, цифри (наприклад, гучність або номери станцій)
            dest[j++] = c;
        } else {
            // Пропускаємо валідний UTF-8
            if (src[i + 1] != '\0') {
                uint8_t next_c = (uint8_t)src[i + 1];
                if ((c >= 0xC2 && c <= 0xDF) && (next_c >= 0x80 && next_c <= 0xBF)) {
                    dest[j++] = c;
                    if (j < dest_size - 1) dest[j++] = next_c;
                    i++;
                    continue;
                }
            }

            // Якщо не UTF-8, конвертуємо з CP1251
            if (j >= dest_size - 2) break; 

            /*if (c >= 0xC0 && c <= 0xDF) { dest[j++] = 0xD0; dest[j++] = c - 0x30; }
            else if (c >= 0xE0 && c <= 0xFF) { dest[j++] = 0xD1; dest[j++] = c - 0x70; }*/
			if (c >= 0xC0 && c <= 0xEF) { // А-Я та а-п
                dest[j++] = 0xD0; 
                dest[j++] = c - 0x30; 
            } 
            else if (c >= 0xF0) { // р-я
                dest[j++] = 0xD1; 
                dest[j++] = c - 0x70; 
            }
            else if (c == 0xB3) { dest[j++] = 0xD1; dest[j++] = 0x96; } 
            else if (c == 0xB2) { dest[j++] = 0xD0; dest[j++] = 0x86; } 
            else if (c == 0xBF) { dest[j++] = 0xD1; dest[j++] = 0x97; } 
            else if (c == 0xAF) { dest[j++] = 0xD0; dest[j++] = 0x87; } 
            else if (c == 0xBA) { dest[j++] = 0xD1; dest[j++] = 0x94; } 
            else if (c == 0xAA) { dest[j++] = 0xD0; dest[j++] = 0x84; } 
            else if (c == 0xB4) { dest[j++] = 0xD2; dest[j++] = 0x91; } 
            else if (c == 0xA5) { dest[j++] = 0xD2; dest[j++] = 0x90; } 
            else { dest[j++] = c; }
        }
    }
    dest[j] = '\0';
}

void handle_command(const char *type, const char *raw_value) {
	char value[512]; 
    
    decode_to_utf8(raw_value, value, sizeof(value));	// Виправляємо кодування за наявності невідповідності UTF-8
	
    if (strcmp(type, "CLI.NAMESET#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "NAMESET: \"%s\"", value);
		strncpy(radio_name, value, sizeof(radio_name));
		radio_name[sizeof(radio_name) - 1] = '\0';
		uint8_t st_num = atoi(value);
		if (radio_st_num != st_num) { 
			radio_isPlaying = false;
			radioMetaClear();
			radio_st_num = st_num;
			xTaskNotifyGive(text_prepare_task_handle);
		}
    }
	else if (strcmp(type, "CLI.META#") == 0) {
        if (true) {
			ESP_LOGI(TAG_UART_HDLR, "META: \"%s\"", value);
			strncpy(radio_meta, value, sizeof(radio_meta));
			radio_meta[sizeof(radio_meta) - 1] = '\0';
			
			char *sep = strstr(radio_meta, " - ");
			if (sep) {
				*sep = '\0';
				strncpy(radio_artist, radio_meta, sizeof(radio_artist) - 1);
				radio_artist[sizeof(radio_artist) - 1] = '\0';
				
				strncpy(radio_title, sep + 3, sizeof(radio_title) - 1); // +3 щоб пропустити " - "
				radio_title[sizeof(radio_title) - 1] = '\0';
			} else {
				radio_artist[0] = '\0';
				strncpy(radio_title, radio_meta, sizeof(radio_title) - 1);
				radio_title[sizeof(radio_title) - 1] = '\0';
			}
		}
    }
	else if (strcmp(type, "CLI.ICY1#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY1: \"%s\"", value);
    }
	else if (strcmp(type, "CLI.ICY2#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY2: \"%s\"", value);
    }
	else if (strcmp(type, "CLI.ICY3#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY3: \"%s\"", value);
    }
	else if (strcmp(type, "CLI.ICY4#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY4: \"%s\"", value);
		strncpy(radio_icy4, value, sizeof(radio_icy4));
		radio_icy4[sizeof(radio_icy4) - 1] = '\0';
    }
	else if (strcmp(type, "CLI.ICY5#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY5: \"%s\"", value);
    }
	else if (strcmp(type, "CLI.ICY6#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY6: \"%s\"", value);
    }
    else if (strcmp(type, "CLI.VOL#") == 0) {
        int vol = atoi(value);
        ESP_LOGI(TAG_UART_HDLR, "VOLUME: %d", vol);
		/*if (vol / 2 != currentSettings.master_volume) {
			if (currentSettings.hp_vol_ctrl) {
				setHeadphonesVolume(vol, true, true);
			} else {
				setSysVolume(vol / 2, 4);
			}
		}*/
    }
    else if (strcmp(type, "CLI.PLAYING#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "PLAYING (значення порожнє)");
		if (mode == MODE_RADIO)
			radio_isPlaying = true;
		else
			uart_cmd_send(KR_STOP);
    }
	else if (strcmp(type, "CLI.STOPPED#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "STOPPED (значення порожнє)");
		radio_isPlaying = false;
		resetScrolling();
    }
	else if (strcmp(type, "CLI.LISTNUM#") == 0) {
		radio_st_qty = atoi(value);
		ESP_LOGI(TAG_UART_HDLR, "LISTNUM: %s", value);
    }
	else if (strcmp(type, "CLI.ICY0#") == 0) {
        ESP_LOGI(TAG_UART_HDLR, "ICY0: \"%s\"", value);
		strncpy(radio_icy0, value, sizeof(radio_icy0));
		radio_icy0[sizeof(radio_icy0) - 1] = '\0';
		if (!radio_ready && strcmp(value, "Ready") == 0) { 
			radio_ready = true;
			uart_cmd_send(KR_INFO);
			//uart_cmd_send(KR_GET_VOL);
			uart_cmd_send(KR_SET_VOL, currentSettings.master_volume * 2);
			uart_cmd_send(KR_LIST);
		}
    }
    else {
        ESP_LOGW(TAG_UART_HDLR, "Невідома команда: %s \"%s\"", type, value);
    }
}

void uart_task(void *pvParameters) {
    uint8_t data[128];

    while (1) {
        int len = uart_read_bytes(UART_PORT, data, sizeof(data), pdMS_TO_TICKS(20));

        if (len > 0) {
            // Лог у текстовому вигляді
            //ESP_LOGI(TAG_UART_HDLR, "RAW: %.*s", len, (char*)data);

            // Лог у hex для відладки "битих" байтів
            //ESP_LOG_BUFFER_HEX(TAG_UART_HDLR, data, len);
			if (DISP_KR_LOG_WHILE_LOADING) {
				strncpy(radio_data, data, sizeof(radio_data));
				radio_data[sizeof(radio_data) - 1] = '\0';
			}

            for (int i = 0; i < len; i++) {
                char c = data[i];
                switch (state) {
                case STATE_WAIT_CLI:
                    if (c == 'C') {
                        type_len = 0;
                        type_buf[type_len++] = c;
                        state = STATE_PARSE_TYPE;
                    }
                    break;
                case STATE_PARSE_TYPE:
                    if (c == ' ' || c == '\n') {
                        state = STATE_WAIT_CLI;
                    } else {
                        type_buf[type_len++] = c;
                        if (type_len >= sizeof(type_buf)-1) {
                            state = STATE_WAIT_CLI;
                        }
                        type_buf[type_len] = '\0';
                        if (c == '#') {
                            state = STATE_SKIP_SPACES;
                        }
                    }
                    break;
                case STATE_SKIP_SPACES:
                    if (c == '\n') {
                        value_buf[0] = '\0';
                        handle_command(type_buf, value_buf);
                        state = STATE_WAIT_CLI;
                    } else if (c == ':' || c == ' ') {
                        // пропускаємо
                    } else {
                        value_len = 0;
                        value_buf[value_len++] = c;
                        state = STATE_READ_VALUE;
                    }
                    break;
                case STATE_READ_VALUE:
					if (c == '\n') {
						// Закриваємо рядок
						value_buf[value_len] = '\0';
						
						// Прибираємо \r і \n з кінця
						size_t vlen = strlen(value_buf);
						while (vlen > 0 && (value_buf[vlen-1] == '\n' || value_buf[vlen-1] == '\r')) {
							value_buf[--vlen] = '\0';
						}

						handle_command(type_buf, value_buf);
						state = STATE_WAIT_CLI;
					} else if (value_len < sizeof(value_buf)-1) {
						value_buf[value_len++] = c;
					} else {
						state = STATE_WAIT_CLI;
					}
					break;
                }
            }
        }

        // Дрібна пауза, щоб не вантажити CPU
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setMode(sys_mode_t mode_) {
	switch (mode) {
		case MODE_BT:
			if (playback_status == ESP_AVRC_PLAYBACK_PLAYING) a2dp_sink.pause();
			break;
		case MODE_RADIO:
			if (radio_isPlaying) uart_cmd_send(KR_STOP);
			break;
		default: break;
	}
	mode = mode_;
	bool s = currentSettings.hp_vol_ctrl && (mode != MODE_BT);
	setSysVolume(currentSettings.master_volume, 6, false);
	setHeadphonesVolume(currentSettings.headphones_volume, s, false);
	resetScrolling();
	dispScreen = SCR_MODE_NAME;
	mode_name_start = esp_timer_get_time() / 1000;
	pt2258.mute(false);
	xTaskNotifyGive(text_prepare_task_handle);
}

int utf8_strlen(const char* str) {
    int len = 0;
    while (*str) {
        uint8_t c = (uint8_t)*str;
        if ((c & 0b11000000) != 0b10000000) {
            // початок UTF-8 символу
            len++;
        }
        str++;
    }
    return len;
}

void printCleanUTF8(VFDisplay& display, const char* str, uint8_t pos) {
    char buf[128]; // Буфер із запасом для багатобайтових UTF-8
    int buf_idx = 0;
    int vis_chars = 0;
    const char* p = str;

    // Копіюємо байти, поки не досягнемо ліміту видимих символів
    while (*p && vis_chars < DISPLAY_DIGITS_NUM) {
        // Перевірка на кастомний код \x (для іконок відтворення)
        if (p[0] == '\\' && p[1] == 'x' && p[2] && p[3]) {
            buf[buf_idx++] = *p++;
            buf[buf_idx++] = *p++;
            buf[buf_idx++] = *p++;
            buf[buf_idx++] = *p++;
        } else {
            // Копіюємо весь UTF-8 символ цілком (від 1 до 4 байтів)
            buf[buf_idx++] = *p++;
            while ((*p & 0b11000000) == 0b10000000) {
                buf[buf_idx++] = *p++;
            }
        }
        vis_chars++;
    }

    // Добиваємо пробілами до кінця екрана (щоб затерти старий текст)
    while (vis_chars < DISPLAY_DIGITS_NUM) {
        buf[buf_idx++] = ' ';
        vis_chars++;
    }
    buf[buf_idx] = '\0';

    display.printUTF8(buf, pos);
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
	//Serial.println(a2dp_sink.to_str(state));
	switch (state) { 
		case ESP_A2D_CONNECTION_STATE_CONNECTED:
			wasConnected = true;
			if (mode == MODE_BT) { dispScreen = SCR_SRC_NAME; }
			break;
		case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
			if (mode == MODE_BT && wasConnected) { dispScreen = SCR_DISCONNECTED; }
			if (playpos != 0) playpos = 0;
			
			strncpy(current_title, "", sizeof(current_title));
			current_title[sizeof(current_title) - 1] = '\0';
			
			strncpy(current_artist, "", sizeof(current_artist));
			current_artist[sizeof(current_artist) - 1] = '\0';
			
			strncpy(current_album, "", sizeof(current_album));
			current_album[sizeof(current_album) - 1] = '\0';
			break;
		default:
			break;
	}
}

void codec_config_callback(uint32_t rate, uint8_t bps, uint8_t channels) {
	if (codec_rate != rate || codec_bps != bps || codec_channels != channels) {
		codec_rate = rate;
		codec_bps = bps;
		codec_channels = channels;
		#if EXT_I2S_CTRL
		cfg.pin_ws = LRCK; // LRCK
		cfg.pin_bck = BCLK;  // BCLK
		cfg.pin_data = DATA;  // DATA
		cfg.sample_rate = rate;
		cfg.channels = channels;
		cfg.bits_per_sample = bps;
		cfg.buffer_count = 8;
		cfg.buffer_size = 256;
		out.begin(cfg);
		ESP_LOGI("I2S", "New I2S config:");
		ESP_LOGI("I2S", "  Sample rate     : %d", cfg.sample_rate);
		ESP_LOGI("I2S", "  Bits per sample : %d", cfg.bits_per_sample);
		ESP_LOGI("I2S", "  Channels        : %d", cfg.channels);
		#endif
	}
}

void avrc_metadata_callback(uint8_t id, const uint8_t* text) {
    if (!text) return;

    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            strncpy(current_title, (const char*)text, MAX_METADATA_LEN - 1);
            current_title[MAX_METADATA_LEN - 1] = '\0';
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            strncpy(current_artist, (const char*)text, MAX_METADATA_LEN - 1);
            current_artist[MAX_METADATA_LEN - 1] = '\0';
            break;
		case ESP_AVRC_MD_ATTR_ALBUM:
            strncpy(current_album, (const char*)text, MAX_METADATA_LEN - 1);
            current_album[MAX_METADATA_LEN - 1] = '\0';
            break;
    }
	
	if (dispScreen == SCR_TITLE_ARTIST ||
		dispScreen == SCR_ALBUM_ARTIST ||
		dispScreen == SCR_PLAY_POS) { 
		resetScrolling();
	}
	need_show_title = true;
}

void avrc_rn_volumechange_callback(int new_vol) {
	if (currentSettings.hp_vol_ctrl) {
		setHeadphonesVolume(new_vol, false, true);
	} else {
		setSysVolume(new_vol, 3);
	}
}

void avrc_rn_playstatus_callback(esp_avrc_playback_stat_t playback) {
	/*if (playback == ESP_AVRC_PLAYBACK_PLAYING && playback_status != ESP_AVRC_PLAYBACK_PLAYING) {
		a2dp_sink.set_volume(currentSettings.master_volume);
	}*/
	playback_status = playback;
	if (playback == ESP_AVRC_PLAYBACK_PAUSED ||
		playback == ESP_AVRC_PLAYBACK_STOPPED ||
		playback == ESP_AVRC_PLAYBACK_FWD_SEEK ||
		playback == ESP_AVRC_PLAYBACK_REV_SEEK) {
			need_show_title = false;
		}
	xTaskNotifyGive(text_prepare_task_handle);
}

void avrc_rn_play_pos_callback(uint32_t play_pos) {
  playpos = play_pos;
}

void avrc_rn_track_change_callback(uint8_t *id) {
  /*ESP_LOGI("AVRC", "Track Change bits:");
  for (uint8_t i = 0; i < 8; i++) {
    ESP_LOGI("AVRC", "\tByte %d : 0x%02X", i, id[i]);
  }

  uint8_t track_change_flag = *id;
  ESP_LOGI("AVRC", "\tFlag value: %d", track_change_flag);*/
  
	if (dispScreen == SCR_TITLE_ARTIST ||
		dispScreen == SCR_ALBUM_ARTIST ||
		dispScreen == SCR_PLAY_POS) { 
		resetScrolling();
	}
  need_show_title = true;
}

// Ця функція вирішує, що робити з поворотом, залежно від режиму
void handle_encoder_action(int steps) {
    switch (current_ui_mode) {
        
        // РЕЖИМ 1: Звичайний (Керування гучністю)
        case UI_MODE_NORMAL: {
			uint8_t curr_volume;
			if (currentSettings.hp_vol_ctrl) {
				curr_volume = currentSettings.headphones_volume;
			} else {
				curr_volume = currentSettings.master_volume;
			}
			
            // Конвертуємо поточну гучність (0..127) у кроки (0..MAX_STEPS)
            int current_steps = (curr_volume * MAX_STEPS + 127 / 2) / 127;
            int new_steps = current_steps + steps;

            // Обмеження
            if (new_steps > MAX_STEPS) new_steps = MAX_STEPS;
            if (new_steps < 0) new_steps = 0;

            // Конвертуємо назад у 0..127 і встановлюємо
			if (currentSettings.hp_vol_ctrl) {
				setHeadphonesVolume((new_steps * 127 + MAX_STEPS / 2) / MAX_STEPS, (mode != MODE_BT), true);
			} else {
				setSysVolume((new_steps * 127 + MAX_STEPS / 2) / MAX_STEPS, 6);
			}
            break;
        }

        // РЕЖИМ 2: Навігація по меню (Заготовка на майбутнє)
        case UI_MODE_SETTINGS:
            // Тут буде код типу: menu_index += steps;
            ESP_LOGI("UI", "Menu scroll: %d", steps);
            break;

        // РЕЖИМ 3: Редагування (Заготовка на майбутнє)
        case UI_MODE_EDIT:
            ESP_LOGI("UI", "Value change: %d", steps);
            break;
    }
}

Button play_button = { BTN_PLAY };
Button next_button = { BTN_NEXT };
Button prev_button = { BTN_PREV };
Button enc_button = { BTN_ENC };

void button_task(void*) {
    // Опитування кожні 20мс (50Гц)
    const TickType_t poll_period = pdMS_TO_TICKS(20); 
    
    // Для кнопок
    const TickType_t ff_interval = pdMS_TO_TICKS(300); 
    const TickType_t rew_interval = pdMS_TO_TICKS(300); 
    TickType_t last_ff_time = 0;
    TickType_t last_rew_time = 0;

    // Локальна змінна для накопичення кроків (щоб не губити залишки ділення на 4)
    int16_t accumulated_steps = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();
        uint32_t now_ms = now * portTICK_PERIOD_MS;
        uint64_t now_real_ms = esp_timer_get_time() / 1000;

        // Оновлюємо кнопки
        play_button.update(now_ms);
        next_button.update(now_ms);
        prev_button.update(now_ms);
        enc_button.update(now_ms);

        // --- ЕНКОДЕР (АПАРАТНИЙ) ---
        // 1. Читаємо "сирі" дані з заліза (скільки набігло за 20мс)
        int16_t delta = encoder.getDelta();
        
        // 2. Додаємо до накопичувача
        accumulated_steps += delta;

        // 3. Перевіряємо на повні кроки (4 імпульси = 1 крок)
        int steps_to_apply = 0;
        
        if (accumulated_steps >= 4 || accumulated_steps <= -4) {
             steps_to_apply = accumulated_steps / 4;
             
             // Скидаємо залишок для "чіткості". 
             // Якщо хочеш плавність - роби: accumulated_steps %= 4;
             accumulated_steps = 0; 
        }

        // 4. Застосовуємо зміни
        if (steps_to_apply != 0) {
            handle_encoder_action(steps_to_apply);
        }

        // --- КНОПКИ ---
        if (enc_button.is_click()) {
            if (current_ui_mode == UI_MODE_NORMAL) {
                if (dispScreen != SCR_VOLUME) {
					if (mode == MODE_BT && a2dp_sink.is_connected()) {
						currentSettings.defScreen++;
						if (currentSettings.defScreen > SCR_CODEC_CFG) {
							currentSettings.defScreen = SCR_TITLE_ARTIST;
						}
						eeprom.update(currentSettings);
						if (currentSettings.defScreen == SCR_TITLE_ARTIST || currentSettings.defScreen == SCR_ALBUM_ARTIST) {
							scr_name_start = esp_timer_get_time() / 1000;
							need_show_scr_name = true;
						}
						resetScrolling();
						dispScreen = currentSettings.defScreen;
						xTaskNotifyGive(text_prepare_task_handle);
					}
				} else {
					currentSettings.hp_vol_ctrl = !currentSettings.hp_vol_ctrl;
					a2dp_sink.set_volume((currentSettings.hp_vol_ctrl) ? currentSettings.headphones_volume : currentSettings.master_volume);
					eeprom.update(currentSettings);
					volume_scr_start = esp_timer_get_time() / 1000;
					xTaskNotifyGive(text_prepare_task_handle);
				}
            }
        }

        switch (mode) {
            case MODE_BT:
                if (play_button.is_click()) {
                     if (playback_status == ESP_AVRC_PLAYBACK_PLAYING) a2dp_sink.pause();
                     else a2dp_sink.play();
                }
                if (play_button.is_hold()) a2dp_sink.disconnect();
                if (next_button.is_click()) a2dp_sink.next();
                if (next_button.check_long_press()) {
                    if ((now - last_ff_time) > ff_interval) {
                        if (!ff_mode) ff_mode = true; 
                        last_ff_time = now;
                        a2dp_sink.fast_forward();
                    }
                } else { ff_mode = false; }
                if (prev_button.is_click()) a2dp_sink.previous();
                if (prev_button.check_long_press()) {
                    if ((now - last_rew_time) > rew_interval) {
                        if (!rew_mode) rew_mode = true;
                        last_rew_time = now;
                        a2dp_sink.rewind();
                    }
                } else { rew_mode = false; }
                break;

            case MODE_RADIO:
                 if (play_button.is_click()) {
                    if (radio_isPlaying) uart_cmd_send(KR_STOP);
                    else uart_cmd_send(KR_START);
                 }
                 if (next_button.is_click()) {
                    if (radio_st_num < radio_st_qty) uart_cmd_send(KR_PLAY_ST_NUM, radio_st_num + 1);
                    else uart_cmd_send(KR_PLAY_ST_NUM, 0);
                    resetScrolling();
                 }
                 if (prev_button.is_click()) {
                    if (radio_st_num != 0) uart_cmd_send(KR_PLAY_ST_NUM, radio_st_num - 1);
                    else uart_cmd_send(KR_PLAY_ST_NUM, radio_st_qty);
                    resetScrolling();
                 }
                 break;
            default: break;
        }
		vTaskDelay(poll_period);
    }
}

void text_prepare_task(void*) {
    while (true) {
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
		if (currentSettings.defScreen == SCR_PLAY_POS) {
			if (need_show_title) {
				if (dispScreen == SCR_PLAY_POS) {
					dispScreen = SCR_TITLE_ARTIST;
				}
			} else if (dispScreen == SCR_TITLE_ARTIST) {
				dispScreen = SCR_PLAY_POS;
			}
		} else if (need_show_title) {
			need_show_title = false;
		}
		switch (dispScreen) {
			case SCR_BT_RDY:
				strncpy(display_buffer, "BT Ready", sizeof(display_buffer));
				display_buffer[sizeof(display_buffer) - 1] = '\0';  // гарантований '\0'
				scrlMode = 0;
				break;
			case SCR_SRC_NAME: {
				const char* name = a2dp_sink.get_peer_name();
				if (name && strcmp(name, "unknown") != 0) {
					snprintf(display_buffer, sizeof(display_buffer), "Connected: %s", name);
				} else {
					strncpy(display_buffer, "Connected", sizeof(display_buffer));
				}
				display_buffer[sizeof(display_buffer) - 1] = '\0';
				scrlMode = 2;
				break; 
			}
			case SCR_TITLE_ARTIST:
				if (currentSettings.defScreen == SCR_TITLE_ARTIST && need_show_scr_name) {
#if DISPLAY_DIGITS_NUM < 14
					strncpy(display_buffer, "Titl/Art", sizeof(display_buffer));
#else
					strncpy(display_buffer, "Title / Artist", sizeof(display_buffer));
#endif
					display_buffer[sizeof(display_buffer) - 1] = '\0';
					scrlMode = 0;
					break;
				} else if (strlen(current_title) > 0 && strlen(current_artist) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s / %s", current_title, current_artist);
				} else if (strlen(current_title) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s", current_title);
				} else if (strlen(current_artist) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s", current_artist);
				} else {
					if (!need_show_title) {
						snprintf(display_buffer, sizeof(display_buffer), "BT Audio");
					} else {
						need_show_title = false;
						skip_title_scr = true;
						break;
					}
				}
				if (need_show_title) {
					scrlMode = 2;
				} else if (utf8_strlen(display_buffer) > DISPLAY_DIGITS_NUM) {  // Виведення з прокруткою або без
					scrlMode = 1;
				} else {
					scrlMode = 0;
				}
				break;
			case SCR_ALBUM_ARTIST:
				if (currentSettings.defScreen == SCR_ALBUM_ARTIST && need_show_scr_name) {
#if DISPLAY_DIGITS_NUM < 14
					strncpy(display_buffer, "Albm/Art", sizeof(display_buffer));
#else
					strncpy(display_buffer, "Album / Artist", sizeof(display_buffer));
#endif
					display_buffer[sizeof(display_buffer) - 1] = '\0';
					scrlMode = 0;
					break;
				} else if (strlen(current_album) > 0 && strlen(current_artist) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s / %s", current_album, current_artist);
				} else if (strlen(current_album) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s", current_album);
				} else if (strlen(current_artist) > 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s", current_artist);
				} else {
					snprintf(display_buffer, sizeof(display_buffer), "BT Audio");
				}
				if (need_show_title) {
					scrlMode = 2;
				} else if (utf8_strlen(display_buffer) > DISPLAY_DIGITS_NUM) {  // Виведення з прокруткою або без
					scrlMode = 1;
				} else {
					scrlMode = 0;
				}
				break;
			case SCR_PLAY_POS: {
				uint32_t total_sec = playpos / 1000;
				uint8_t playpos_sec = total_sec % 60;
				uint8_t playpos_min = (total_sec / 60) % 60;
				uint16_t playpos_hour = total_sec / 3600;
				const char* pb_stat_symbol = "";

#if DISPLAY_CGROM_TABLE == 2
				// =====================================
				// СТАРА ТАБЛИЦЯ (General-purpose 02)
				// =====================================
				if (ff_mode) {
					pb_stat_symbol = "\x0D";
				} else if (rew_mode) {
					pb_stat_symbol = "\x0C";
				} else {
					switch (playback_status) {
						case ESP_AVRC_PLAYBACK_STOPPED:		pb_stat_symbol = "\x16"; break;
						case ESP_AVRC_PLAYBACK_PLAYING:		pb_stat_symbol = "\x0B"; break;
						case ESP_AVRC_PLAYBACK_PAUSED:		pb_stat_symbol = "\x0E"; break;
						case ESP_AVRC_PLAYBACK_FWD_SEEK:	pb_stat_symbol = "\x0D"; break;
						case ESP_AVRC_PLAYBACK_REV_SEEK:	pb_stat_symbol = "\x0C"; break;
						default:							pb_stat_symbol = " ";    break;
					}
				}
#else
				// =====================================
				// НОВА ТАБЛИЦЯ (General-purpose 07)
				// =====================================
				if (ff_mode) {
					pb_stat_symbol = "\\xBB";
				} else if (rew_mode) {
					pb_stat_symbol = "\\xAB";
				} else {
					switch (playback_status) {
						case ESP_AVRC_PLAYBACK_STOPPED:		pb_stat_symbol = "\\x04"; break; // Іконка STOP (CGRAM)
						case ESP_AVRC_PLAYBACK_PLAYING:		pb_stat_symbol = "\\x02"; break; // Іконка PLAY (CGRAM)
						case ESP_AVRC_PLAYBACK_PAUSED:		pb_stat_symbol = "\\x03"; break; // Іконка PAUSE (CGRAM)
						case ESP_AVRC_PLAYBACK_FWD_SEEK:	pb_stat_symbol = "\\xBB"; break; // Символ '»'
						case ESP_AVRC_PLAYBACK_REV_SEEK:	pb_stat_symbol = "\\xAB"; break; // Символ '«'
						default:							pb_stat_symbol = " ";     break;
					}
				}
#endif

				char time_str[16];
				if (playpos_hour > 0) {
					snprintf(time_str, sizeof(time_str), "%d:%02d:%02d", playpos_hour, playpos_min, playpos_sec);
				} else {
					snprintf(time_str, sizeof(time_str), "%02d:%02d", playpos_min, playpos_sec);
				}

#if DISPLAY_DIGITS_NUM == 16
				snprintf(display_buffer, sizeof(display_buffer), "%s%15s", pb_stat_symbol, time_str);
#else
				snprintf(display_buffer, sizeof(display_buffer), "%s%7s", pb_stat_symbol, time_str);
#endif
				
				//snprintf(display_buffer, sizeof(display_buffer), "%s  %02d:%02d", pb_stat_symbol, playpos_min, playpos_sec);
				display_buffer[sizeof(display_buffer) - 1] = '\0';
				scrlMode = 0;
				break;
			}
			case SCR_DISCONNECTED:
				strncpy(display_buffer, "Disconnected", sizeof(display_buffer));
				display_buffer[sizeof(display_buffer) - 1] = '\0';  // гарантований '\0'
				scrlMode = 2;
				break;
			case SCR_VOLUME: {
				switch (currentSettings.hp_vol_ctrl) {
					case false: {
						int vol = (currentSettings.master_volume * MAX_STEPS + 127 / 2) / 127;
#if DISPLAY_DIGITS_NUM == 16
						if (vol < 10) {
							snprintf(display_buffer, sizeof(display_buffer), "   Volume   %d   ", vol);
						} else {
							snprintf(display_buffer, sizeof(display_buffer), "   Volume  %d   ", vol);
						}
#else
						if (vol < 10) {
							snprintf(display_buffer, sizeof(display_buffer), "VOL    %d", vol);
						} else {
							snprintf(display_buffer, sizeof(display_buffer), "VOL   %d", vol);
						}
#endif
						break;
					}
					case true: {
						int vol = (currentSettings.headphones_volume * MAX_STEPS + 127 / 2) / 127;
#if DISPLAY_DIGITS_NUM == 16
						if (vol < 10) {
							snprintf(display_buffer, sizeof(display_buffer), " Headphones   %d ", vol);
						} else {
							snprintf(display_buffer, sizeof(display_buffer), " Headphones  %d ", vol);
						}
#else
						if (vol < 10) {
							snprintf(display_buffer, sizeof(display_buffer), "HP     %d", vol);
						} else {
							snprintf(display_buffer, sizeof(display_buffer), "HP    %d", vol);
						}
#endif
						break;
					}
				}
				display_buffer[sizeof(display_buffer) - 1] = '\0';
				scrlMode = 0;
				break;
			}
			case SCR_CODEC_CFG: {
				// Розрахунок з десятковою частиною
				int rate_khz_int = codec_rate / 1000;
				int rate_khz_frac = (codec_rate % 1000) / 100;
				
				if (rate_khz_frac != 0) {
					snprintf(display_buffer, sizeof(display_buffer), "%s, %d.%d kHz, %d bits", current_codec, rate_khz_int, rate_khz_frac, codec_bps);
				} else {
					snprintf(display_buffer, sizeof(display_buffer), "%s, %d kHz, %d bits", current_codec, rate_khz_int, codec_bps);
				}
				display_buffer[sizeof(display_buffer) - 1] = '\0';
				scrlMode = 2;  // одноразова прокрутка
				break;
			}
			case SCR_RADIO_TITLE:
				if (radio_ready) {
					if (radio_isPlaying) {
						if (radio_title[0] != '\0' && radio_artist[0] != '\0') {
							snprintf(display_buffer, sizeof(display_buffer), "%s / %s", radio_title, radio_artist);
						} else if (radio_title[0] != '\0') {
							strncpy(display_buffer, radio_title, sizeof(display_buffer));
						} else if (radio_icy0[0] != '\0') {
							strncpy(display_buffer, radio_icy0, sizeof(display_buffer));
						} else {
							strncpy(display_buffer, radio_name, sizeof(display_buffer));
						}
						
						if (utf8_strlen(display_buffer) > DISPLAY_DIGITS_NUM) {
							scrlMode = 1;
						} else {
							scrlMode = 0;
						}
					} else {
						strncpy(display_buffer, radio_name, sizeof(display_buffer));
						scrlMode = 0;
					}
				} else {
					if (DISP_KR_LOG_WHILE_LOADING) {
						strncpy(display_buffer, radio_data, sizeof(display_buffer));
					} else if (radio_icy4[0] == '\0') {
						strncpy(display_buffer, radio_icy0, sizeof(display_buffer));
					} else {
						snprintf(display_buffer, sizeof(display_buffer), "%s %s", radio_icy0, radio_icy4);
					}
					if (utf8_strlen(display_buffer) > DISPLAY_DIGITS_NUM) {
						scrlMode = 1;
					} else {
						scrlMode = 0;
					}
				}
				display_buffer[sizeof(display_buffer) - 1] = '\0';
				break;
			case SCR_MODE_NAME:
				switch (mode) {
					case MODE_BT:
						snprintf(display_buffer, sizeof(display_buffer), "BT Audio");
						break;
					case MODE_RADIO:
						snprintf(display_buffer, sizeof(display_buffer), "KaRadio ");
						break;
					case MODE_IN1:
						snprintf(display_buffer, sizeof(display_buffer), "Input 1 ");
						break;
					case MODE_IN2:
						snprintf(display_buffer, sizeof(display_buffer), "Input 2 ");
						break;
					case MODE_IN3:
						snprintf(display_buffer, sizeof(display_buffer), "Input 3 ");
						break;
					default: break;
				}
				scrlMode = 0;
				break;
		}
        //vTaskDelay(pdMS_TO_TICKS(500));
		//UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
		//ESP_LOGI("STACK", "text_prepare_task stack high water mark: %d", highWaterMark);
    }
}

void display_task(void*){
	while (true) {
		if (skip_title_scr) {
			skip_title_scr = false;
			xTaskNotifyGive(text_prepare_task_handle);
			continue;
		}
		if (need_show_scr_name) {
			if (currentSettings.defScreen == SCR_TITLE_ARTIST || currentSettings.defScreen == SCR_ALBUM_ARTIST) {
				if (esp_timer_get_time() / 1000 - scr_name_start > SCREEN_NAME_SHOW_INT) {
					need_show_scr_name = false;
					xTaskNotifyGive(text_prepare_task_handle);
				}
			} else {
				need_show_scr_name = false;
			}
		}
		switch (scrlMode) {
			case 0:
				printCleanUTF8(vfdisplay, display_buffer, 0);
				if (dispScreen == SCR_VOLUME && esp_timer_get_time() / 1000 - volume_scr_start > VOL_SCR_SHOW_INT) {
					dispScreen = lastScreen;
					xTaskNotifyGive(text_prepare_task_handle);
				}
				if (dispScreen == SCR_MODE_NAME && esp_timer_get_time() / 1000 - mode_name_start > MODE_SCR_SHOW_INT) {
					switch (mode) {
						case MODE_BT:
							if (a2dp_sink.is_connected()) {
								dispScreen = currentSettings.defScreen;
							} else {
								dispScreen = SCR_BT_RDY;
							}
							break;
						case MODE_RADIO:
							dispScreen = SCR_RADIO_TITLE;
							break;
						default:
							break;
					}
					xTaskNotifyGive(text_prepare_task_handle);
				}
				break;
			case 1:
				printScrollingUTF8(vfdisplay, display_buffer, 0);
				break;
			case 2:
				if (printScrollingUTF8(vfdisplay, display_buffer, 2)) {
				switch (dispScreen) {
					case SCR_SRC_NAME:
						dispScreen = currentSettings.defScreen;
						break;
					case SCR_DISCONNECTED:
						dispScreen = SCR_BT_RDY;
						break;
					case SCR_CODEC_CFG:
						dispScreen = SCR_TITLE_ARTIST;
						currentSettings.defScreen = SCR_TITLE_ARTIST;
						break;
					case SCR_TITLE_ARTIST:
						need_show_title = false;
						break;
					default:
						break;
				}
				xTaskNotifyGive(text_prepare_task_handle);
				}
				break;
		}
		//UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
		//ESP_LOGI("STACK", "display_task stack high water mark: %d", highWaterMark);
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

extern "C" void app_main(void)
{
	main_task_handle = xTaskGetCurrentTaskHandle();
    #if !EXT_I2S_CTRL
	I2SConfig i2s_config;
    i2s_config.pin_ws = LRCK; // LRCK
    i2s_config.pin_bck = BCLK;  // BCLK
    i2s_config.pin_data = DATA;  // DATA
	i2s_config.buffer_count = 8; 
    i2s_config.buffer_size = 256;
	//i2s_config.sample_rate = 44100; 
    //i2s_config.bits_per_sample = 16;
	i2s_config.use_apll = true;

    out.begin(i2s_config);
	#endif
	
	gpio_config();
	#if USE_I2C
	i2c_init();
	#endif
	#if USE_Eeprom24C
	eeprom.begin();
	#endif
	uart_init();
    vfdisplay.begin(DISPLAY_DIGITS_NUM, 140);  // Немаксимальна яскравість для захисту від вигорання
	// Завантажуємо символи у вільні комірки CGRAM
	vfdisplay.setCustomChar(0x00, char_YE);       // Код 0x00 буде 'Є'
	vfdisplay.setCustomChar(0x01, char_ye_small); // Код 0x01 буде 'є'
	vfdisplay.setCustomChar(0x02, icon_play);     // Код 0x02 буде 'PLAY'
	vfdisplay.setCustomChar(0x03, icon_pause);    // Код 0x03 буде 'PAUSE'
	vfdisplay.setCustomChar(0x04, icon_stop);     // Код 0x04 буде 'STOP'
	
	#if USE_PT2258 
    if (pt2258.init() == ESP_OK) {
        ESP_LOGI(TAG_PT2258, "PT2258 Initialized!");
        pt2258.setVolume(79);
    } else {
        ESP_LOGE(TAG_PT2258, "Failed to find PT2258!");
    }
	#endif
	
	vTaskDelay(pdMS_TO_TICKS(100));
	
	#if EXT_I2S_CTRL
	a2dp_sink.set_stream_reader(read_data_stream, false);
	#endif
	a2dp_sink.set_i2s_ringbuffer_size(256 * 1024);
    a2dp_sink.set_i2s_stack_size(4 * 1024); // Збільшуємо стек задачі I2S з 2048 до 10000. Це прибере помилку "Guru Meditation Error".
	a2dp_sink.set_i2s_ringbuffer_prefetch_percent(20);
	//a2dp_sink.set_i2s_write_size_upto(511 * 8);
	
	if (USE_HW_VOL_CTRL_BT) {
		a2dp_sink.set_volume_control(&no_volume_control);
	}
	
    a2dp_sink.set_auto_reconnect(true);
    a2dp_sink.set_task_core(1);
	a2dp_sink.set_task_priority(20);
	a2dp_sink.set_on_connection_state_changed(connection_state_changed);
	a2dp_sink.set_codec_config_callback(codec_config_callback);
	a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
	a2dp_sink.set_avrc_rn_volumechange(avrc_rn_volumechange_callback);
	a2dp_sink.set_avrc_rn_playstatus_callback(avrc_rn_playstatus_callback);
	a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback, 1);
	a2dp_sink.set_avrc_rn_track_change_callback(avrc_rn_track_change_callback);
    a2dp_sink.start(BT_NAME_STR);
	
	#if EXT_I2S_CTRL
	// setup output
	//auto cfg = out.defaultConfig();
	cfg.pin_ws = LRCK; // LRCK
    cfg.pin_bck = BCLK;  // BCLK
    cfg.pin_data = DATA;  // DATA
	cfg.sample_rate = 44100;
	cfg.channels = 2;
	cfg.bits_per_sample = 16;
	cfg.buffer_count = 8;
	cfg.buffer_size = 256;
	out.begin(cfg);
	#endif
	
	if (eeprom.load(currentSettings) != ESP_OK || currentSettings.magic_byte != 0xA6) {
        // Якщо EEPROM чиста або сталася помилка - ставимо дефолт
        ESP_LOGE("Eeprom24C", "Failed to load settings!");
		currentSettings.magic_byte = 0xA6;
        currentSettings.master_volume = 20;
		currentSettings.defScreen = SCR_TITLE_ARTIST;
		currentSettings.headphones_volume = 20;
		currentSettings.hp_vol_ctrl = false;
        
        // Можна одразу зберегти дефолт, щоб наступного разу було чисто
        eeprom.forceSave(currentSettings);
    }
	
	ESP_LOGI("MEM", "Total PSRAM: %d", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	
	//a2dp_sink.set_volume(127);
	//volume = a2dp_sink.get_volume();
	//setSysVolume(currentSettings.master_volume, 6, false); // Перенесено в button_task, щоб встановити гучність з врахуванням джерела
	
	xTaskCreatePinnedToCore(uart_task, "uart_task", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(button_task, "button_task", 4096, NULL, 5, &button_task_handle, 0);
	xTaskCreatePinnedToCore(text_prepare_task, "text_prepare_task", 4096, NULL, 1, &text_prepare_task_handle, 0);
    xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, 1, NULL, 0);
	
	// Читаємо початковий стан перемикача при запуску
    // Даємо трохи часу напрузі стабілізуватися
    vTaskDelay(pdMS_TO_TICKS(50));
    
    uint8_t initial_sw_state = (gpio_get_level(ROT_SW_BT_RAD) << 2) | 
                               (gpio_get_level(ROT_SW_B) << 1) | 
                               gpio_get_level(ROT_SW_A);

	// Встановлюємо початковий режим
    if (initial_sw_state < 5) {
        // Встановлюємо режим без зайвого Mute, бо ми тільки ввімкнулись
        ESP_LOGI("MAIN", "Initial Mode Set: %d (State: %d)", sw_state_to_mode[initial_sw_state], initial_sw_state);
        setMode(sw_state_to_mode[initial_sw_state]);
    } else {
		// Неприпустимий стан перемикача режимів, примусове встановлення режиму MODE_BT, щоб завершити ініціалізацію
		ESP_LOGE("MAIN", "Mode switch invalid state! Initial Mode Set: %d", MODE_BT);
        setMode(MODE_BT);
	}
    
    // Запам'ятовуємо цей стан як поточний
    uint8_t last_sw_state = initial_sw_state;
	
	while(1) {
        // Чекаємо ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // --- КРОК 1: МИТТЄВА ДІАГНОСТИКА ---
        uint8_t current_state = (gpio_get_level(ROT_SW_BT_RAD) << 2) | 
                                (gpio_get_level(ROT_SW_B) << 1) | 
                                gpio_get_level(ROT_SW_A);
        
        uint8_t delayed_state = 0; 

        // СЦЕНАРІЙ А: Швидке перемикання (Aux1 -> Aux2 і т.д.)
        if (current_state != last_sw_state) {
             //ESP_LOGI("MAIN", "Fast change detected! Muting.");
             goto CONFIRMED_MUTE; 
        }

        // --- КРОК 2: ПЕРЕВІРКА НА "ПОВІЛЬНИЙ ФРОНТ" (BT -> Radio) ---
        // Якщо ми тут, значить ISR був, але пін наче старий.
        
        vTaskDelay(pdMS_TO_TICKS(15)); // Чекаємо 15 мс

        // Тут вже просто присвоюємо значення, а не оголошуємо
        delayed_state = (gpio_get_level(ROT_SW_BT_RAD) << 2) | 
                        (gpio_get_level(ROT_SW_B) << 1) | 
                        gpio_get_level(ROT_SW_A);

        // Якщо і тепер нічого - це точно шум.
        if (delayed_state == last_sw_state) {
             // Ігноруємо
             xTaskNotifyStateClear(main_task_handle);
             gpio_intr_enable(ROT_SW_BT_RAD);
             gpio_intr_enable(ROT_SW_A);
             gpio_intr_enable(ROT_SW_B);
             continue; 
        }
        
        // СЦЕНАРІЙ Б: Повільне перемикання знайдено!
        current_state = delayed_state;
        ESP_LOGI("MAIN", "Slow change detected (BT->Radio). Muting.");

        // --- СПІЛЬНА ТОЧКА ВХОДУ ДЛЯ MUTE ---
        CONFIRMED_MUTE:
        
        // 1. ГЛУШИМО
        pt2258.mute(true);

        // 2. Основний час на брязкіт
        vTaskDelay(pdMS_TO_TICKS(150)); 

        // 3. Фіналізація
        uint8_t stable_state = (gpio_get_level(ROT_SW_BT_RAD) << 2) | 
                               (gpio_get_level(ROT_SW_B) << 1) | 
                               gpio_get_level(ROT_SW_A);

        if (stable_state != last_sw_state && stable_state < 5) {
             ESP_LOGI("MAIN", "Mode Switch: %d -> %d", last_sw_state, stable_state);
             setMode(sw_state_to_mode[stable_state]); 
             last_sw_state = stable_state;
        } 
        else {
             // Хибна тривога після дебаунсу
             pt2258.mute(false);
        }

        xTaskNotifyStateClear(main_task_handle);
        gpio_intr_enable(ROT_SW_BT_RAD);
        gpio_intr_enable(ROT_SW_A);
        gpio_intr_enable(ROT_SW_B);
    }
}
