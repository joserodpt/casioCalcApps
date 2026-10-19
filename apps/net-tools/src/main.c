#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "info.h"
#include "keys.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR   C_RGB(3, 14, 20)
#define HOSTC   C_RGB(28, 29, 31)   /* background of host bits */

/* Fields being edited: the four octets, then the prefix length */
#define FIELD_PREFIX 4
#define FIELD_COUNT  5

typedef struct {
    int field[FIELD_COUNT];
    int sel;
    int typed;          /* digits typed into the selected field */
} state_t;

//---
//  Address helpers
//---

static uint32_t address(state_t const *s)
{
    return ((uint32_t)s->field[0] << 24) | (s->field[1] << 16)
        | (s->field[2] << 8) | s->field[3];
}

static void set_address(state_t *s, uint32_t a)
{
    for (int i = 0; i < 4; i++) s->field[i] = (a >> (24 - 8 * i)) & 0xff;
}

static uint32_t netmask(int prefix)
{
    return prefix ? 0xffffffffu << (32 - prefix) : 0;
}

/* "a.b.c.d" */
static char *ip_text(char *buf, uint32_t a)
{
    sprintf(buf, "%u.%u.%u.%u", (unsigned)(a >> 24), (unsigned)(a >> 16)
        & 0xff, (unsigned)(a >> 8) & 0xff, (unsigned)a & 0xff);
    return buf;
}

/* Decimal text of a 64-bit count (printf has no %llu) */
static char *u64_text(char *buf, uint64_t v)
{
    char tmp[24];
    int n = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    for (int i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
    return buf;
}

static char address_class(uint32_t a)
{
    int top = a >> 24;
    return top < 128 ? 'A' : top < 192 ? 'B' : top < 224 ? 'C'
        : top < 240 ? 'D' : 'E';
}

static bool in(uint32_t a, uint32_t net, int prefix)
{
    return (a & netmask(prefix)) == net;
}

/* Special-purpose ranges (RFC 6890 and friends) */
static char const *address_type(uint32_t a)
{
    if (a == 0xffffffffu)           return L("Broadcast", "Broadcast");
    if (in(a, 0x00000000, 8))       return L("This network", "Esta rede");
    if (in(a, 0x0a000000, 8))       return L("Private", "Privado");
    if (in(a, 0x64400000, 10))      return L("Shared (CGNAT)", "Partilhado CGNAT");
    if (in(a, 0x7f000000, 8))       return L("Loopback", "Loopback");
    if (in(a, 0xa9fe0000, 16))      return L("Link-local", "Link-local");
    if (in(a, 0xac100000, 12))      return L("Private", "Privado");
    if (in(a, 0xc0000200, 24))      return L("Documentation", "Documentacao");
    if (in(a, 0xc0a80000, 16))      return L("Private", "Privado");
    if (in(a, 0xc6120000, 15))      return L("Benchmarking", "Benchmarking");
    if (in(a, 0xc6336400, 24))      return L("Documentation", "Documentacao");
    if (in(a, 0xcb007100, 24))      return L("Documentation", "Documentacao");
    if (in(a, 0xe0000000, 4))       return L("Multicast", "Multicast");
    if (in(a, 0xf0000000, 4))       return L("Reserved", "Reservado");
    return L("Public", "Publico");
}

//---
//  Editing
//---

static int field_max(int f)
{
    return f == FIELD_PREFIX ? 32 : 255;
}

static void select_field(state_t *s, int f)
{
    s->sel = (f + FIELD_COUNT) % FIELD_COUNT;
    s->typed = 0;
}

/* The first digit replaces the field; the field is left when full */
static void type_digit(state_t *s, int d)
{
    int *v = &s->field[s->sel];
    int max = field_max(s->sel);

    if (s->typed == 0) *v = d;
    else if (*v * 10 + d <= max) *v = *v * 10 + d;
    else return;
    s->typed++;

    if (*v * 10 > max || s->typed == (s->sel == FIELD_PREFIX ? 2 : 3)) {
        if (s->sel < FIELD_PREFIX) select_field(s, s->sel + 1);
        else s->typed = 0;
    }
}

static void delete_digit(state_t *s)
{
    int *v = &s->field[s->sel];
    if (*v == 0 && s->sel > 0) {
        select_field(s, s->sel - 1);
        s->typed = 1;   /* keep editing the previous field */
        return;
    }
    *v /= 10;
    s->typed = *v ? 1 : 0;
}

static void set_prefix(state_t *s, int prefix)
{
    if (prefix < 0) prefix = 0;
    if (prefix > 32) prefix = 32;
    s->field[FIELD_PREFIX] = prefix;
}

/* Move to the previous or next subnet of the same size */
static void step_subnet(state_t *s, int dir)
{
    int prefix = s->field[FIELD_PREFIX];
    if (prefix == 0) return;
    uint32_t block = (uint32_t)1 << (32 - prefix);   /* 0 for /0 */
    uint32_t net = address(s) & netmask(prefix);
    set_address(s, dir > 0 ? net + block : net - block);
}

//---
//  Rendering
//---

#define INPUT_Y   22
#define ROWS_Y    46
#define ROW_STEP  13
#define VALUE_X   92
#define GRID_X    48
#define GRID_Y    146
#define CELL      10        /* cell pitch; cells are 9x12 */
#define OCTET_GAP 6

/* Address being typed, in the large font; the selected field is
   underlined (dimmed until a digit is typed, as it will be replaced) */
static void draw_input(state_t const *s)
{
    font_t const *old = dfont(&font_big);
    int x = 10;

    for (int f = 0; f < FIELD_COUNT; f++) {
        char text[4];
        int w;
        sprintf(text, "%d", s->field[f]);
        dsize(text, NULL, &w, NULL);

        bool sel = (f == s->sel);
        dtext(x, INPUT_Y, sel ? COLOR : C_BLACK, text);
        if (sel) drect(x, INPUT_Y + 15, x + w - 1, INPUT_Y + 16,
            s->typed ? COLOR : C_LIGHT);
        x += w + 2;

        if (f < 3) dtext(x, INPUT_Y, C_DARK, ".");
        else if (f == 3) dtext(x, INPUT_Y, C_DARK, "/");
        x += f < 3 ? 9 : 13;
    }
    dfont(old);
}

static void row(int i, char const *label, char const *value, int color)
{
    int y = ROWS_Y + i * ROW_STEP;
    dtext(10, y, C_DARK, label);
    dtext(VALUE_X, y, color, value);
}

static void note(int i, char const *text)
{
    dtext_opt(DWIDTH - 10, ROWS_Y + i * ROW_STEP, C_DARK, C_NONE,
        DTEXT_RIGHT, DTEXT_TOP, text);
}

static void draw_results(state_t const *s)
{
    char a[20], b[20], text[48];
    uint32_t ip = address(s);
    int prefix = s->field[FIELD_PREFIX];
    uint32_t mask = netmask(prefix);
    uint32_t net = ip & mask;
    uint32_t bcast = net | ~mask;
    uint64_t total = (uint64_t)1 << (32 - prefix);

    row(0, L("Address", "Endereco"), ip_text(a, ip), C_BLACK);
    sprintf(text, L("Class %c, %s", "Classe %c, %s"), address_class(ip), address_type(ip));
    note(0, text);

    row(1, L("Netmask", "Mascara"), ip_text(a, mask), C_BLACK);
    sprintf(text, "0x%08X", (unsigned)mask);
    note(1, text);
    row(2, L("Wildcard", "Curinga"), ip_text(a, ~mask), C_BLACK);

    sprintf(text, "%s/%d", ip_text(a, net), prefix);
    row(3, L("Network", "Rede"), text, COLOR);
    row(4, "Broadcast", prefix < 31 ? ip_text(a, bcast) : L("none", "nenhum"), COLOR);

    /* /31 links use both addresses (RFC 3021), /32 is a single host */
    uint64_t usable;
    if (prefix == 32) {
        usable = 1;
        row(5, "Host", ip_text(a, ip), C_BLACK);
    }
    else if (prefix == 31) {
        usable = 2;
        sprintf(text, "%s - %s", ip_text(a, net), ip_text(b, bcast));
        row(5, "Hosts", text, C_BLACK);
    }
    else {
        usable = total - 2;
        sprintf(text, "%s - %s", ip_text(a, net + 1), ip_text(b, bcast - 1));
        row(5, "Hosts", text, C_BLACK);
    }

    row(6, L("Usable", "Utilizaveis"), u64_text(a, usable), C_BLACK);
    sprintf(text, L("of %s address%s", "de %s endereco%s"), u64_text(b, total),
        total == 1 ? "" : "es");
    note(6, text);
}

static int cell_x(int bit)
{
    int c = 31 - bit;   /* column, most significant bit first */
    return GRID_X + c * CELL + (c / 8) * OCTET_GAP;
}

/* One row of 32 bits; network bits are filled with the app colour */
static void draw_bits(int y, char const *label, uint32_t v, int prefix)
{
    dtext_opt(GRID_X - 6, y + 2, C_DARK, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
        label);
    for (int bit = 31; bit >= 0; bit--) {
        int x = cell_x(bit);
        bool network = (31 - bit) < prefix;
        int one = (v >> bit) & 1;
        drect(x, y, x + CELL - 2, y + 11, network ? COLOR : HOSTC);
        dtext_opt(x + 4, y + 2, network ? C_WHITE : C_BLACK, C_NONE,
            DTEXT_CENTER, DTEXT_TOP, one ? "1" : "0");
    }
}

static void draw_grid(state_t const *s)
{
    int prefix = s->field[FIELD_PREFIX];
    uint32_t ip = address(s);
    char text[8];

    draw_bits(GRID_Y, "IP", ip, prefix);
    draw_bits(GRID_Y + 15, "Mask", netmask(prefix), prefix);

    /* Boundary between network and host bits */
    int bx = prefix == 0 ? cell_x(31) - 2
        : prefix == 32 ? cell_x(0) + CELL
        : (cell_x(32 - prefix) + CELL - 1 + cell_x(31 - prefix)) / 2;
    drect(bx - 1, GRID_Y - 3, bx, GRID_Y + 29, C_RGB(29, 6, 4));
    sprintf(text, "/%d", prefix);
    int lx = bx < DWIDTH - 16 ? bx : DWIDTH - 16;   /* stay on screen */
    dtext_opt(lx, GRID_Y + 32, C_RGB(29, 6, 4), C_NONE, DTEXT_CENTER,
        DTEXT_TOP, text);

    /* Octet values under each group */
    for (int o = 0; o < 4; o++) {
        int x = cell_x(31 - 8 * o) + 4 * CELL - 1;
        if (abs(x - bx) < 22) continue;     /* keep the /n label readable */
        sprintf(text, "%u", (unsigned)(ip >> (24 - 8 * o)) & 0xff);
        dtext_opt(x, GRID_Y + 32, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            text);
    }
}

static void draw(state_t const *s)
{
    dclear(C_WHITE);
    info_title(L("IP subnet calculator", "Calc. de sub-rede"), COLOR);

    draw_input(s);
    dline(6, ROWS_Y - 5, DWIDTH - 7, ROWS_Y - 5, C_LIGHT);
    draw_results(s);
    dline(6, GRID_Y - 7, DWIDTH - 7, GRID_Y - 7, C_LIGHT);
    draw_grid(s);

    fkey_button(1, L("PREV", "ANT"));
    fkey_button(2, L("NEXT", "SEG"));
    int prefixes[4] = { 8, 16, 24, 30 };
    for (int i = 0; i < 4; i++) {
        char label[4];
        sprintf(label, "/%d", prefixes[i]);
        if (s->field[FIELD_PREFIX] == prefixes[i])
            fkey_button_color(i + 3, label, COLOR);
        else fkey_button(i + 3, label);
    }
    dupdate();
}

//---
//  Main loop
//---

int main(void)
{
    state_t s = { .field = { 192, 168, 1, 10, 24 }, .sel = 0 };

    lang_init();
    usb_screen_init();

    while (1) {
        draw(&s);

        int key = getkey().key;
        int d = key_digit(key);

        switch (key) {
        case KEY_EXIT:
            return 1;
        case INFO_KEY:
            info_show(L("NetTools", "Redes"), L("IP subnet calc. " APPS_VERSION, "Calc. sub-rede " APPS_VERSION), COLOR);
            break;
        case KEY_LEFT:
            select_field(&s, s.sel - 1);
            break;
        case KEY_RIGHT:
        case KEY_DOT:
        case KEY_EXE:
            select_field(&s, s.sel + 1);
            break;
        case KEY_DIV:
            select_field(&s, FIELD_PREFIX);
            break;
        case KEY_UP:
            set_prefix(&s, s.field[FIELD_PREFIX] + 1);
            break;
        case KEY_DOWN:
            set_prefix(&s, s.field[FIELD_PREFIX] - 1);
            break;
        case KEY_DEL:
            delete_digit(&s);
            break;
        case KEY_ACON:
            /* Clear the address, keep the prefix */
            memset(s.field, 0, 4 * sizeof *s.field);
            select_field(&s, 0);
            break;
        case KEY_F1:
            step_subnet(&s, -1);
            break;
        case KEY_F2:
            step_subnet(&s, +1);
            break;
        case KEY_F3: set_prefix(&s, 8); break;
        case KEY_F4: set_prefix(&s, 16); break;
        case KEY_F5: set_prefix(&s, 24); break;
        case KEY_F6: set_prefix(&s, 30); break;
        default:
            if (d >= 0) type_digit(&s, d);
            break;
        }
    }
}
