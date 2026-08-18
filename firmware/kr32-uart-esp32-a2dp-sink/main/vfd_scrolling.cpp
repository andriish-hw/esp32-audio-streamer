#include "vfd_scrolling.h"
#include "vfd_display_utils.h"
#include <vector>
#include <string>
#include <cstring>
#include "esp_timer.h"

#define SCROLL_HOLD_TIME 1500		// мс на початку і в кінці (нульова позиція)
#define SCROLL_STEP_TIME 150		// мс між кроками
#define SCROLL_CYCLE_PAUSE 5000		// мс перед новим циклом (крім першого; SCROLL_HOLD_TIME не діє)
#define DISPLAY_WIDTH 8

using std::vector;
using std::string;

static bool need_scrl_rst = false; 

void resetScrolling() {
	need_scrl_rst = true;
}

bool printScrollingUTF8(VFDisplay& display, const char* str, uint8_t scrollMode, uint8_t startPos, uint8_t scrollWidth) {
    static uint32_t lastScrollTime = 0;
    static int scrollPos = 0;
    static bool scrollHold = false;
    static bool finishedScroll = false;
    static bool needHold = false; // новий прапорець
    static vector<uint8_t> vfdChars;
    static std::string prevStr = "";
    static uint8_t prevScrollMode = 255;

    // Якщо рядок або режим змінився чи викликано скидання - скидаємо стан
    if (scrollMode != prevScrollMode || prevStr != std::string(str) || need_scrl_rst) {
        scrollPos = 0;
        scrollHold = false;
        needHold = false;
        finishedScroll = false;
        lastScrollTime = esp_timer_get_time() / 1000;
        prevStr = str;
        prevScrollMode = scrollMode;
		need_scrl_rst = false;

        vfdChars.clear();

        // Розбити UTF-8 -> Unicode -> VFD-коди
        const char* p = str;
        while (*p) {
            uint32_t cp;
            size_t len = utf8_to_unicode((const uint8_t*)p, &cp);
            if (len == 0) len = 1;
            uint8_t vfd = unicode_to_vfd(cp);
            vfdChars.push_back(vfd);
            p += len;
        }
    }

    // Пауза на кожному поверненні до початкового положення (позиція 0)
    if (scrollPos == 1 && !needHold) {
        scrollHold = true;
        needHold = true;
        lastScrollTime = esp_timer_get_time() / 1000;
    }

    // Виконуємо затримку прокрутки
    if (scrollHold) {
        if ((esp_timer_get_time() / 1000) - lastScrollTime < (finishedScroll ? SCROLL_CYCLE_PAUSE : SCROLL_HOLD_TIME)) return false;
        scrollHold = false;
        lastScrollTime = esp_timer_get_time() / 1000;
    }

    // Прокручуємо текст
    if ((esp_timer_get_time() / 1000) - lastScrollTime >= SCROLL_STEP_TIME) {
        lastScrollTime = esp_timer_get_time() / 1000;

        // Вивести scrollWidth символів
        for (int i = 0; i < scrollWidth; i++) {
            int idx = scrollPos + i;
            if (idx >= 0 && idx < (int)vfdChars.size()) {
                display.putChar(startPos + i, vfdChars[idx]);
            } else {
                display.putChar(startPos + i, ' ');
            }
        }

        scrollPos++;

        if (scrollPos > (int)vfdChars.size()) {
            scrollPos = -scrollWidth; // виїзд з правого краю зони прокрутки
            scrollHold = false;
            needHold = false;
            finishedScroll = true;
        }
    }

	display.show();

    if (scrollMode == 2) {
		return finishedScroll;
	} else {
		return false;
	}
}
