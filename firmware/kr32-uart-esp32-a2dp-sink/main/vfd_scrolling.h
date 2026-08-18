#ifndef VFD_SCROLLING_H
#define VFD_SCROLLING_H

#include <stdint.h>
#include "vfd_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Виводить текст із прокруткою на VFD-дисплей
 * 
 * @param display Об'єкт класу VFDisplay
 * @param str UTF-8 рядок, який буде прокручуватись
 * @param scrollMode 0 — постійна прокрутка, 1 — циклічна, 2 — один раз
 * @param startPos Позиція на дисплеї, з якої починається прокрутка (за замовчуванням 0)
 * @param scrollWidth Кількість символів, відведених під прокрутку (за замовчуванням 16)
 * @return true якщо прокрутка завершена (тільки в режимі 2), інакше false
 */

void resetScrolling();

bool printScrollingUTF8(VFDisplay& display, const char* str, uint8_t scrollMode, uint8_t startPos = 0, uint8_t scrollWidth = 16);

#ifdef __cplusplus
}
#endif

#endif // VFD_SCROLLING_H
