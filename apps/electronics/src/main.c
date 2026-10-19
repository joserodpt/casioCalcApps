#include <gint/display.h>

#include "menu.h"
#include "tools.h"
#include "version.h"
#include "i18n.h"

#define COLOR   C_RGB(20, 9, 3)

static menu_item_t const tools[] = {
    { "Colour code", "Codigo de cores",
      "Bands to value, value to bands, E12/E24/E96",
      "Bandas <-> valor, E12/E24/E96",
      colorcode_run, C_RGB(20, 9, 3) },
    { "SMD codes", "Codigos SMD", "472, 4R7, 4702 and EIA-96 (01C) codes",
      "472, 4R7, 4702 e EIA-96 (01C)",
      smd_run, C_RGB(4, 14, 14) },
    { "Series / parallel", "Serie / paralelo",
      "Up to 8 resistors in series and parallel",
      "Ate 8 resistencias em serie e paralelo",
      rnet_run, C_RGB(3, 9, 24) },
    { "Voltage divider", "Divisor de tensao",
      "Vout, current, power, R2 for a target",
      "Vout, corrente, potencia, R2 alvo",
      divider_run, C_RGB(2, 17, 6) },
    { "RC / RL filters", "Filtros RC / RL",
      "Cutoff frequency, time constant, LC",
      "Frequencia de corte, constante, LC",
      filter_run, C_RGB(17, 4, 22) },
    { "555 timer", "Temporizador 555",
      "Astable frequency and duty, monostable pulse",
      "Frequencia astavel, pulso monoestavel",
      timer555_run, C_RGB(26, 9, 0) },
    { "LED resistor", "Resistencia LED",
      "Series resistor, power and E-series values",
      "Resistencia serie, potencia, serie E",
      led_run, C_RGB(24, 3, 6) },
};

static menu_t const menu = {
    "Electronics", "Toolkit " APPS_VERSION, "Ferramentas " APPS_VERSION,
    COLOR, tools, (int)(sizeof tools / sizeof *tools),
};

int main(void)
{
    lang_init();
    menu_run(&menu);
    return 1;
}
