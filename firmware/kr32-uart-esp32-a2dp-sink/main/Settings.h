#pragma once
#include <stdint.h>

// Описуємо, що саме ми зберігаємо
typedef struct {
    uint8_t magic_byte;     // Щоб знати, чи це перший запуск (напр. 0xA5)
    uint8_t master_volume;  // 0..127
	uint8_t defScreen;
	uint8_t headphones_volume;
	bool hp_vol_ctrl;		// 0 - ручка гучності регулює головний вихід, 1 - навушники
    // ... будь-які інші поля ...
} SystemSettings;