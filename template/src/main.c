#include <gint/display.h>
#include <gint/keyboard.h>

#include "utils.h"

int main(void)
{
    dclear(C_WHITE);
    row_title("__APP_NAME__");
    row_print(2, 1, "Hello from __APP_NAME__!");
    row_print(4, 1, "Press EXIT to quit.");
    dupdate();

    while(getkey().key != KEY_EXIT) {}
    return 1;
}
