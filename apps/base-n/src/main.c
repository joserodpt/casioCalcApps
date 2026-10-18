#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"

//REFERENCIA: https://git.planet-casio.com/Lephenixnoir/gint/src/branch/master/include/gint

enum { MODE_DEC, MODE_HEX, MODE_BIN, MODE_COUNT };

typedef struct {
    char const *name;
    int base;
    int max_digits;     // longest input accepted in this mode
} base_t;

static base_t const bases[MODE_COUNT] = {
    { "DEC", 10, 19 },  // 10^19 - 1 < 2^64, so it can never overflow
    { "HEX", 16, 16 },  // 16 hex digits = 64 bits
    { "BIN",  2, 32 },  // limited by the screen width
};

#define MAX_INPUT_LENGTH 32
#define BIN_ROW_DIGITS   32
#define FORMAT_BUF_SIZE  65 // 64 binary digits + '\0'

static char const digits[] = "0123456789ABCDEF";

// Parse an already-validated digit string (input length is capped per base,
// so the result always fits in 64 bits)
static uint64_t parse(char const *str, int base)
{
    uint64_t value = 0;
    for (; *str; str++)
        value = value * base + (strchr(digits, *str) - digits);
    return value;
}

// Format value into buf; returns a pointer to the first digit inside buf
static char *format(uint64_t value, int base, char buf[FORMAT_BUF_SIZE])
{
    char *p = buf + FORMAT_BUF_SIZE - 1;
    *p = '\0';
    do {
        *--p = digits[value % base];
        value /= base;
    } while (value);
    return p;
}

// Print the binary representation, wrapping over two rows if needed
static void print_binary(int row, uint64_t value)
{
    char buf[FORMAT_BUF_SIZE];
    char *bin = format(value, 2, buf);
    int len = strlen(bin);

    if (len > BIN_ROW_DIGITS) {
        row_print(row + 1, 2, "%s", bin + len - BIN_ROW_DIGITS);
        bin[len - BIN_ROW_DIGITS] = '\0';
    }
    row_print(row, 2, "%s", bin);
}

// Digit value of a key (0-15), or -1 if the key is not a digit
static int key_digit(int key)
{
    switch (key) {
        case KEY_0: return 0;
        case KEY_1: return 1;
        case KEY_2: return 2;
        case KEY_3: return 3;
        case KEY_4: return 4;
        case KEY_5: return 5;
        case KEY_6: return 6;
        case KEY_7: return 7;
        case KEY_8: return 8;
        case KEY_9: return 9;
        case KEY_XOT: return 10;
        case KEY_LOG: return 11;
        case KEY_LN:  return 12;
        case KEY_SIN: return 13;
        case KEY_COS: return 14;
        case KEY_TAN: return 15;
        default: return -1;
    }
}

int main(void)
{
    int mode = MODE_DEC;
    char input[MAX_INPUT_LENGTH + 1] = "";
    int length = 0;

    while (1) {
        base_t const *b = &bases[mode];
        uint64_t value = parse(input, b->base);
        char buf[FORMAT_BUF_SIZE];
        int row = 3;

        // Update the display
        dclear(C_WHITE);
        row_title("Base-N Converter      v1.0       Coded in Portugal :)");
        row_print(1, 2, "Input > %s", input);

        if (mode != MODE_DEC)
            row_print(row++, 2, "Decimal: %s", format(value, 10, buf));
        if (mode != MODE_HEX)
            row_print(row++, 2, "Hexadecimal: 0x%s", format(value, 16, buf));
        if (mode != MODE_BIN) {
            row_print(row++, 2, "Binary:");
            print_binary(row, value);
        }

        for (int i = 0; i < MODE_COUNT; i++) {
            if (i == mode) fkey_button_selected(i + 1, bases[i].name);
            else fkey_button(i + 1, bases[i].name);
        }

        dupdate();

        // getkey() only returns KEYEV_DOWN and KEYEV_HOLD (repeat), so
        // holding DEL deletes repeatedly
        int key = getkey().key;
        int d;

        if (key == KEY_EXIT) {
            break;
        } else if (key == KEY_F1 || key == KEY_F2 || key == KEY_F3) {
            mode = key - KEY_F1;
            input[0] = '\0'; length = 0;
        } else if (key == KEY_DEL) {
            if (length > 0) input[--length] = '\0';
        } else if ((d = key_digit(key)) >= 0 && d < b->base
                && length < b->max_digits) {
            input[length++] = digits[d];
            input[length] = '\0';
        }
    }

    return 1;
}
