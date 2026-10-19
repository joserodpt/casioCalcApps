#include <gint/keyboard.h>

#include "keys.h"

int key_hex(int key)
{
    switch (key) {
        case KEY_0: return 0; case KEY_1: return 1; case KEY_2: return 2;
        case KEY_3: return 3; case KEY_4: return 4; case KEY_5: return 5;
        case KEY_6: return 6; case KEY_7: return 7; case KEY_8: return 8;
        case KEY_9: return 9;
        case KEY_XOT: return 10; case KEY_LOG: return 11;
        case KEY_LN: return 12; case KEY_SIN: return 13;
        case KEY_COS: return 14; case KEY_TAN: return 15;
        default: return -1;
    }
}

int key_digit(int key)
{
    int d = key_hex(key);
    return d < 10 ? d : -1;
}
