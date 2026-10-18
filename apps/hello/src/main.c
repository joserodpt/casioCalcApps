#include <gint/display.h>
#include <gint/keyboard.h>

#include "utils.h"
#include "info.h"

#define VERSION "v1.0"
#define COLOR   C_BLUE

static void draw(void)
{
    dclear(C_WHITE);
    info_title("Hello", COLOR);
    row_print(2, 1, "Hello from Hello!");
    row_print(4, 1, "Press EXIT to quit.");
    dupdate();
}

int main(void)
{
    while(1) {
        draw();

        int key = getkey().key;
        if(key == KEY_EXIT) return 1;
        if(key == INFO_KEY) info_show("Hello", VERSION, COLOR);
    }
}
