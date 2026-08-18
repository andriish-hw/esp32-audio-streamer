#ifndef VFD_DISPLAY_UTILS_H
#define VFD_DISPLAY_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Перетворення UTF-8 байтів у Unicode codepoint
 * 
 * @param input Вказівник на вхідний UTF-8 буфер
 * @param codepoint Вказівник на uint32_t, куди буде записано результат
 * @return Кількість зчитаних байтів (1–4), або 0 при помилці
 */
size_t utf8_to_unicode(const uint8_t *input, uint32_t *codepoint);

/**
 * @brief Перетворення Unicode codepoint у VFD-код символу
 * 
 * @param cp Unicode символ
 * @return VFD-код для відображення
 */
uint8_t unicode_to_vfd(uint32_t cp);

#ifdef __cplusplus
}
#endif

#endif // VFD_DISPLAY_UTILS_H
