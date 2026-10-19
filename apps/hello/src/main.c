#include <gint/display.h>
#include <gint/keyboard.h>

#include "info.h"
#include "version.h"
#include "i18n.h"

#define COLOR   C_BLUE

static void draw(void)
{
    dclear(C_WHITE);
    info_title("Hello", COLOR);
    dtext(6, 36, C_BLACK, L("Hello from Hello!", "Ola do Hello!"));
    dtext(6, 64, C_BLACK, L("Press EXIT to quit.", "Prima EXIT para sair."));
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
