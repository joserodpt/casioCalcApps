#include <gint/display.h>
#include <gint/keyboard.h>

#include "utils.h"
#include "info.h"
#include "ui.h"
#include "tools.h"
#include "version.h"
#include "i18n.h"

#define COLOR   C_RGB(20, 9, 3)

typedef struct {
    char const *name, *name_pt;
    char const *description, *description_pt;
    int color;
    void (*run)(void);
} tool_t;

static tool_t const tools[] = {
    { "Colour code", "Codigo de cores",
      "Bands to value, value to bands, E12/E24/E96",
      "Bandas <-> valor, E12/E24/E96",
      C_RGB(20, 9, 3), colorcode_run },
    { "SMD codes", "Codigos SMD", "472, 4R7, 4702 and EIA-96 (01C) codes",
      "472, 4R7, 4702 e EIA-96 (01C)",
      C_RGB(4, 14, 14), smd_run },
    { "Series / parallel", "Serie / paralelo",
      "Up to 8 resistors in series and parallel",
      "Ate 8 resistencias em serie e paralelo",
      C_RGB(3, 9, 24), rnet_run },
    { "Voltage divider", "Divisor de tensao",
      "Vout, current, power, R2 for a target",
      "Vout, corrente, potencia, R2 alvo",
      C_RGB(2, 17, 6), divider_run },
    { "RC / RL filters", "Filtros RC / RL",
      "Cutoff frequency, time constant, LC",
      "Frequencia de corte, constante, LC",
      C_RGB(17, 4, 22), filter_run },
    { "555 timer", "Temporizador 555",
      "Astable frequency and duty, monostable pulse",
      "Frequencia astavel, pulso monoestavel",
      C_RGB(26, 9, 0), timer555_run },
    { "LED resistor", "Resistencia LED",
      "Series resistor, power and E-series values",
      "Resistencia serie, potencia, serie E",
      C_RGB(24, 3, 6), led_run },
};
#define TOOL_COUNT (int)(sizeof tools / sizeof *tools)

#define MENU_Y    20
#define MENU_STEP 26

static void draw(int sel)
{
    dclear(C_WHITE);
    info_title("Electronics", COLOR);

    for (int i = 0; i < TOOL_COUNT; i++) {
        tool_t const *t = &tools[i];
        int y = MENU_Y + i * MENU_STEP;

        if (i == sel) drect(0, y, DWIDTH - 1, y + MENU_STEP - 2,
            C_RGB(28, 29, 31));
        drect(8, y + 3, 26, y + 21, t->color);
        dprint_opt(17, y + 8, C_WHITE, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            "%d", i + 1);
        dtext(36, y + 3, i == sel ? t->color : C_BLACK,
            lang_current == L_PT ? t->name_pt : t->name);
        etext(36, y + 14, C_DARK, DTEXT_LEFT, false,
            lang_current == L_PT ? t->description_pt : t->description);
        if (i == sel) dtext_opt(DWIDTH - 10, y + 8, t->color, C_NONE,
            DTEXT_RIGHT, DTEXT_TOP, ">");
    }
    dupdate();
}

int main(void)
{
    int sel = 0;

    lang_init();

    while (1) {
        draw(sel);

        int key = getkey().key;
        int open = -1;

        switch (key) {
        case KEY_EXIT:
            return 1;
        case INFO_KEY:
            info_show("Electronics", L("Toolkit " APPS_VERSION,
                "Ferramentas " APPS_VERSION), COLOR);
            break;
        case KEY_UP:
            sel = (sel + TOOL_COUNT - 1) % TOOL_COUNT;
            break;
        case KEY_DOWN:
            sel = (sel + 1) % TOOL_COUNT;
            break;
        case KEY_EXE:
        case KEY_RIGHT:
            open = sel;
            break;
        case KEY_1: open = 0; break;
        case KEY_2: open = 1; break;
        case KEY_3: open = 2; break;
        case KEY_4: open = 3; break;
        case KEY_5: open = 4; break;
        case KEY_6: open = 5; break;
        case KEY_7: open = 6; break;
        }

        if (open >= 0 && open < TOOL_COUNT) {
            sel = open;
            tools[open].run();
        }
    }
}
