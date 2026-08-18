#include "vfd_display.h"
#include <cctype>
#include <cstdlib>

#define VFD_ROM_TABLE 7

size_t utf8_to_unicode(const uint8_t *input, uint32_t *codepoint) {
    uint8_t b0 = input[0];

    if (b0 <= 0x7F) {
        *codepoint = b0;
        return 1;
    } else if ((b0 & 0xE0) == 0xC0) {
        *codepoint = ((b0 & 0x1F) << 6) | (input[1] & 0x3F);
        return 2;
    } else if ((b0 & 0xF0) == 0xE0) {
        *codepoint = ((b0 & 0x0F) << 12) | ((input[1] & 0x3F) << 6) | (input[2] & 0x3F);
        return 3;
    } else if ((b0 & 0xF8) == 0xF0) {
        *codepoint = ((b0 & 0x07) << 18) | ((input[1] & 0x3F) << 12) |
                     ((input[2] & 0x3F) << 6) | (input[3] & 0x3F);
        return 4;
    }

    return 0; // Invalid UTF-8
}

#if VFD_ROM_TABLE == 2
// ==========================================
// General-purpose code: 02
// ==========================================
uint8_t unicode_to_vfd(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return cp;

    switch (cp) {
        case 0x0: return ' ';
		case 0x00C0: return 0xD0;
		case 0x00C1: return 'A';
		case 0x00C2: return 0xD5;
		case 0x00C3: return 0xD5;
		case 0x00C4: return 0x8E;
		case 0x00C5: return 0x8F;
		case 0x00C6: return 0x92;
		case 0x00C7: return 0x80;
		case 0x00C8: return 0xD1;
		case 0x00C9: return 0x90;
		case 0x00CA: return 0xD6;
		case 0x00CB: return 0xD3;
		case 0x00CC: return 0x8D;
		case 0x00CD: return 0xA1;
		case 0x00CE: return 0xD7;
		case 0x00CF: return 0xD4;
		case 0x00D1: return 0xA5;
		case 0x00D2: return 0x95;
		case 0x00D3: return 0xA2;
		case 0x00D4: return 0xD8;
		case 0x00D5: return 0xD8;
		case 0x00D6: return 0x99;
		case 0x00D8: return 'O';
		case 0x00D9: return 0xD2;
		case 0x00DA: return 0xA3;
		case 0x00DB: return 0xD9;
		case 0x00DC: return 0x9A;
		case 0x00DD: return 'Y';
		case 0x00DF: return 0xB1;
		
		case 0x00E0: return 0x85;
		case 0x00E1: return 0xA0;
		case 0x00E2: return 0x83;
		case 0x00E3: return 0x83;
		case 0x00E4: return 0x84;
		case 0x00E5: return 0x86;
		case 0x00E6: return 0x91;
		case 0x00E7: return 0x87;
		case 0x00E8: return 0x8A;
		case 0x00E9: return 0x82;
		case 0x00EA: return 0x88;
		case 0x00EB: return 0x89;
		case 0x00EC: return 0x8D;
		case 0x00ED: return 0xA1;
		case 0x00EE: return 0x8C;
		case 0x00EF: return 0x8B;
		case 0x00F1: return 0xA4;
		case 0x00F2: return 0x95;
		case 0x00F3: return 0xA2;
		case 0x00F4: return 0x93;
		case 0x00F5: return 0x93;
		case 0x00F6: return 0x94;
		case 0x00F8: return 'o';
		case 0x00F9: return 0x97;
		case 0x00FA: return 0xA3;
		case 0x00FB: return 0x96;
		case 0x00FC: return 0x81;
		case 0x00FD: return 'y';
		case 0x00FF: return 0x98;
		
		case 0x0102: return 0xD5;
		case 0x0103: return 0x83;
		case 0x0104: return 'A';
		case 0x0105: return 'a';
		case 0x0106:
		case 0x010C: return 'C';
		case 0x0107: 
		case 0x010D: return 'c';
		case 0x010E: return 'D';
		case 0x010F: return 'd';
		case 0x0118: 
		case 0x011A: return 'E';
		case 0x0119: 
		case 0x011B: return 'e';
		case 0x0141: return 'L';
		case 0x0142: return 'l';
		
        case 0x0411: return 0xDA;
        case 0x0413: return 0xDB;
        case 0x0414: return 0xDC;
        case 0x0404: return 0xB4;
        case 0x0416: return 0xDD;
        case 0x0417: return 0xDE;
        case 0x0418: return 0xDF;
        case 0x0407: return 0xD4;
        case 0x0419: return 0xE0;
        case 0x041B: return 0xE1;
        case 0x041F: return 0xE2;
        case 0x0423: return 0xE3;
        case 0x0424: return 0xE4;
        case 0x0426: return 0xE5;
        case 0x0427: return 0xE6;
        case 0x0428: return 0xE7;
        case 0x0429: return 0xE8;
        case 0x042A: return 0xE9;
        case 0x042B: return 0xEA;
        case 0x042C: return 0xE9;
        case 0x042D: return 0xEB;
        case 0x042E: return 0xEC;
        case 0x042F: return 0xED;
        case 0x0490: return 0xDB;
        case 0x0410: return 'A';
        case 0x0412: return 'B';
        case 0x0415: return 'E';
        case 0x0406: return 'I';
        case 0x041A: return 'K';
        case 0x041C: return 'M';
        case 0x041D: return 'H';
        case 0x041E: return 'O';
        case 0x0420: return 'P';
        case 0x0421: return 'C';
        case 0x0422: return 'T';
        case 0x0425: return 'X';
        case 0x0431: return 0xDA;
        case 0x0433: return 0xDB;
        case 0x0434: return 0xDC;
        case 0x0454: return 0xB4;
        case 0x0436: return 0xDD;
        case 0x0437: return 0xDE;
        case 0x0438: return 0xDF;
        case 0x0457: return 0xD4;
        case 0x0439: return 0xE0;
        case 0x043B: return 0xE1;
        case 0x043F: return 0xE2;
        case 0x0443: return 0xE3;
        case 0x0444: return 0xE4;
        case 0x0446: return 0xE5;
        case 0x0447: return 0xE6;
        case 0x0448: return 0xE7;
        case 0x0449: return 0xE8;
        case 0x044A: return 0xE9;
        case 0x044B: return 0xEA;
        case 0x044C: return 0xE9;
        case 0x044D: return 0xEB;
        case 0x044E: return 0xEC;
        case 0x044F: return 0xED;
        case 0x0491: return 0xDB;
        case 0x0430: return 'A';
        case 0x0432: return 'B';
        case 0x0435: return 'E';
        case 0x0456: return 'I';
        case 0x043A: return 'K';
        case 0x043C: return 'M';
        case 0x043D: return 'H';
        case 0x043E: return 'O';
        case 0x0440: return 'P';
        case 0x0441: return 'C';
        case 0x0442: return 'T';
        case 0x0445: return 'X';
		
		case 0x2010:
		case 0x2011:
		case 0x2013:
		case 0x2014:
		case 0x2015:
		case 0x2212: return '-';
		case 0x2053: return '~';
		case 0x301C: return 0xF5;
        default: 
			if (cp <= 0xFF) {
				return cp;
			} else {
				return ' ';
			}				
    }
}

#elif VFD_ROM_TABLE == 7
// ==========================================
// General-purpose code: 07
// (плюс 'Є' і 'є' в CGRAM 0 і 1
// ==========================================
uint8_t unicode_to_vfd(uint32_t cp) {
    // 1. Базовий ASCII (включає цифри та стандартну латиницю)
    if (cp >= 0x20 && cp <= 0x7E) return cp;

    // 2. Латиниця з діакритикою (ISO 8859-1 Supplement)
    // Таблиця VFD ідеально збігається з юнікодом у цьому діапазоні (0xC0 - 0xFF)
    if (cp >= 0x00C0 && cp <= 0x00FF) return cp;

    // 3. Унікальні та кириличні символи
    switch (cp) {
        // --- Латиниця розширена (Транслітерація) ---
        case 0x0102: case 0x0104: return 'A';
        case 0x0103: case 0x0105: return 'a';
        case 0x0106: case 0x010C: return 'C';
        case 0x0107: case 0x010D: return 'c';
        case 0x010E: return 'D';
        case 0x010F: return 'd';
        case 0x0118: case 0x011A: return 'E';
        case 0x0119: case 0x011B: return 'e';
        case 0x0141: return 'L';
        case 0x0142: return 'l';

        // --- Українські літери ---
        case 0x0406: return 'I';      // І (Англійська I)
        case 0x0456: return 'i';      // і (Англійська i)
        case 0x0407: return 0xCF;     // Ї -> Ï (Latin-1 Supplement)
        case 0x0457: return 0xEF;     // ї -> ï (Latin-1 Supplement)
        case 0x0404: return 0x00;     // Є -> CGRAM RAM0
        case 0x0454: return 0x01;     // є -> CGRAM RAM1
        case 0x0490: return 0x09;     // Ґ -> Г (Візуальна заміна)
        case 0x0491: return 0x1F;     // ґ -> г (Візуальна заміна)

        // --- Великі кириличні літери (А-Я) ---
        case 0x0410: return 'A';
        case 0x0411: return 0x08;     // Б
        case 0x0412: return 'B';
        case 0x0413: return 0x09;     // Г
        case 0x0414: return 0x0A;     // Д
        case 0x0415: return 'E';
        case 0x0416: return 0x0B;     // Ж
        case 0x0417: return 0x0C;     // З
        case 0x0418: return 0x0D;     // И
        case 0x0419: return 0x0E;     // Й
        case 0x041A: return 'K';
        case 0x041B: return 0x0F;     // Л
        case 0x041C: return 'M';
        case 0x041D: return 'H';
        case 0x041E: return 'O';
        case 0x041F: return 0x10;     // П 
        case 0x0420: return 'P';
        case 0x0421: return 'C';
        case 0x0422: return 'T';
        case 0x0423: return 0x11;     // У 
        case 0x0424: return 0x12;     // Ф 
        case 0x0425: return 'X';
        case 0x0426: return 0x13;     // Ц 
        case 0x0427: return 0x14;     // Ч 
        case 0x0428: return 0x15;     // Ш 
        case 0x0429: return 0x16;     // Щ 
        case 0x042A: return 0x17;     // Ъ 
        case 0x042B: return 0x18;     // Ы
        case 0x042C: return 0x19;     // Ь
        case 0x042D: return 0x1A;     // Э
        case 0x042E: return 0x1B;     // Ю
        case 0x042F: return 0x1C;     // Я

        // --- Малі кириличні літери (а-я) ---
        case 0x0430: return 'a';
        case 0x0431: return 0x1D;     // б
        case 0x0432: return 0x1E;     // в
        case 0x0433: return 0x1F;     // г
        case 0x0434: return 0x80;     // д
        case 0x0435: return 'e';
        case 0x0436: return 0x81;     // ж
        case 0x0437: return 0x82;     // з
        case 0x0438: return 0x83;     // и
        case 0x0439: return 0x84;     // й
        case 0x043A: return 0x85;     // к 
        case 0x043B: return 0x86;     // л
        case 0x043C: return 0x87;     // м 
        case 0x043D: return 0x88;     // н 
        case 0x043E: return 'o';
        case 0x043F: return 0x89;     // п
        case 0x0440: return 'p';
        case 0x0441: return 0x8A;     // с
        case 0x0442: return 0x8B;     // т 
        case 0x0443: return 'y';      // у
        case 0x0444: return 0x8D;     // ф
        case 0x0445: return 'x';
        case 0x0446: return 0x8E;     // ц
        case 0x0447: return 0x8F;     // ч
        case 0x0448: return 0x90;     // ш
        case 0x0449: return 0x91;     // щ
        case 0x044A: return 0x92;     // ъ
        case 0x044B: return 0x93;     // ы
        case 0x044C: return 0x94;     // ь
        case 0x044D: return 0x95;     // э
        case 0x044E: return 0x96;     // ю
        case 0x044F: return 0x97;     // я

        // --- Тире та спецсимволи ---
        case 0x2010: 
        case 0x2011: 
        case 0x2013: 
        case 0x2014: 
        case 0x2015: 
        case 0x2212: return '-';
        case 0x2053: return '~';
        
        default: 
            if (cp <= 0xFF) return cp;
            return ' ';               
    }
}
#endif