#include <gint/display.h>
#include <gint/keyboard.h>

#include "utils.h"
#include "info.h"
#include "version.h"
#include "i18n.h"

#define COLOR   C_BLUE

static void draw(void)
{
    dclear(C_WHITE);
    info_title("Hello", COLOR);
    row_print(2, 1, L("Hello from Hello!", "Ola do Hello!"));
    row_print(4, 1, L("Press EXIT to quit.", "Prima EXIT para sair."));
    dupdate();
}

int main(void)
{
    lang_init();

    while(1) {
        draw();

        int key = getkey().key;
        if(key == KEY_EXIT) return 1;
        if(key == INFO_KEY) info_show("Hello", APPS_VERSION, COLOR);
    }
}
