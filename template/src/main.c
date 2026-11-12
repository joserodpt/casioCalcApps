#include <gint/display.h>
#include <gint/keyboard.h>

#include "info.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR   C_BLUE

static void draw(void)
{
    dclear(C_WHITE);
    info_title("__APP_NAME__", COLOR);
#if GINT_HW_FX
    dtext(2, 14, C_BLACK, L("Hello from __APP_NAME__!",
        "Ola do __APP_NAME__!", "Bonjour de __APP_NAME__!"));
    dtext(2, 24, C_DARK, L("Press EXIT to quit.", "Prima EXIT para sair.",
        "EXIT pour quitter."));
#else
    dtext(6, 36, C_BLACK, L("Hello from __APP_NAME__!",
        "Ola do __APP_NAME__!", "Bonjour de __APP_NAME__!"));
    dtext(6, 64, C_BLACK, L("Press EXIT to quit.", "Prima EXIT para sair.",
        "EXIT pour quitter."));
#endif
    dupdate();
}

int main(void)
{
    lang_init();
    usb_screen_init();
    screen_init();

    while(1) {
        draw();

        int key = getkey().key;
        if(key == KEY_EXIT) return 1;
        if(key == INFO_KEY) info_show("__APP_NAME__", APPS_VERSION, COLOR);
    }
}
