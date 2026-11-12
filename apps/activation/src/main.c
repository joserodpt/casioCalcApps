/* Activation: a product-activation demo tied to the calculator.

   The request code is 6 hex digits drawn from a salted hash of the
   calculator's product ID (8 bytes of ROM at 0x8001FFD0), so it is the
   same on every run of one calculator and different on another. The
   activation code is a hash of the request code with a second salt:
   tools/keygen.py computes it on a PC, and the add-in checks the one typed
   by computing it too. Both hashes must match keygen.py's.

   A good code is kept in \\fls0\Activ.key and checked again at start-up,
   so the add-in stays activated, but not on a calculator the file is
   copied to. */

#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "keys.h"
#include "info.h"
#include "version.h"
#include "i18n.h"
#include "usb_screen.h"

#define COLOR      C_RGB(22, 12, 2)
#define GOOD       C_RGB(2, 20, 6)
#define BAD        C_RGB(26, 2, 2)
#define BOXC       C_RGB(28, 28, 28)

#define PRODUCT_ID 0x8001FFD0u          /* 8 bytes in the OS's ROM */
#define CODE_LEN   6
#define SAVE_PATH  "/Activ.key"         /* \\fls0\Activ.key */

#define SALT_REQUEST  "casioCalcApps/activation/request"
#define SALT_ACTIVATE "casioCalcApps/activation/activate"

//---
//  Codes (the same steps as tools/keygen.py)
//---

/* FNV-1a over the salt then the data, then MurmurHash3's finaliser so
   that every input bit moves the top 24 bits, which give the code */
static uint32_t hash(char const *salt, uint8_t const *data, int n)
{
    uint32_t h = 2166136261u;
    for (char const *s = salt; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    for (int i = 0; i < n; i++)
        h = (h ^ data[i]) * 16777619u;
    h ^= h >> 16; h *= 0x85ebca6bu;
    h ^= h >> 13; h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h >> 8;
}

static void code_text(uint32_t v, char *out)
{
    sprintf(out, "%06X", (unsigned)(v & 0xFFFFFF));
}

static char request[CODE_LEN + 1];  /* this calculator's */
static char expected[CODE_LEN + 1]; /* the activation code for it */

static void make_codes(void)
{
    uint8_t id[8];
    for (int i = 0; i < 8; i++)
        id[i] = *(volatile uint8_t *)(uintptr_t)(PRODUCT_ID + i);
    code_text(hash(SALT_REQUEST, id, 8), request);
    code_text(hash(SALT_ACTIVATE, (uint8_t const *)request, CODE_LEN),
        expected);
}

static bool saved_ok(void)
{
    char s[CODE_LEN + 1] = { 0 };
    FILE *fp = fopen(SAVE_PATH, "rb");
    if (!fp) return false;
    /* Size 1: this libc's fread returns bytes, not items */
    bool ok = fread(s, 1, CODE_LEN, fp) == CODE_LEN;
    fclose(fp);
    return ok && !strcmp(s, expected);
}

static void save(char const *code)
{
    FILE *fp = fopen(SAVE_PATH, "wb");
    if (!fp) return;
    fwrite(code, 1, CODE_LEN, fp);
    fclose(fp);
}

//---
//  Screens
//---

/* One character per box, the cursor's box outlined in the app's colour */
static void code_boxes(int y, char const *s, int cursor, int color)
{
    int const w = 26, gap = 6;
    int x = (DWIDTH - CODE_LEN * w - (CODE_LEN - 1) * gap) / 2;
    font_t const *old = dfont(&font_big);
    for (int i = 0; i < CODE_LEN; i++, x += w + gap) {
        drect(x, y, x + w - 1, y + 29, BOXC);
        if (i == cursor)
            drect_border(x, y, x + w - 1, y + 29, C_NONE, 2, COLOR);
        if (s[i]) {
            char c[2] = { s[i], 0 };
            dtext_opt(x + w / 2, y + 15, color, C_NONE, DTEXT_CENTER,
                DTEXT_MIDDLE, c);
        }
    }
    dfont(old);
}

static void draw_request(char const *typed, bool wrong)
{
    dclear(C_WHITE);
    info_title(L("Activation", "Ativacao", "Activation"), COLOR);

    dtext_opt(DWIDTH / 2, 24, C_BLACK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Request code", "Codigo de pedido", "Code de demande"));
    code_boxes(38, request, -1, COLOR);
    dtext_opt(DWIDTH / 2, 74, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Send it to get this calculator's activation code.",
          "Envie-o para receber o codigo de ativacao.",
          "Envoyez-le pour obtenir le code d'activation."));

    dtext_opt(DWIDTH / 2, 100, C_BLACK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Activation code", "Codigo de ativacao", "Code d'activation"));
    int n = strlen(typed);
    code_boxes(114, typed, n < CODE_LEN ? n : -1,
        wrong ? BAD : C_BLACK);

    if (wrong)
        dtext_opt(DWIDTH / 2, 152, BAD, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("Wrong code: check it and try again.",
              "Codigo errado: verifique-o e tente de novo.",
              "Code incorrect: verifiez-le et reessayez."));
    else
        dtext_opt(DWIDTH / 2, 152, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
            L("A-F: X,theta,T  log  ln  sin  cos  tan.  EXE: activate",
              "A-F: X,theta,T  log  ln  sin  cos  tan.  EXE: ativar",
              "A-F: X,theta,T  log  ln  sin  cos  tan.  EXE: activer"));

    fkey_button(1, L("CLEAR", "LIMPAR", "EFFACER"));
    fkey_button(6, L("ABOUT", "SOBRE", "INFOS"));
    dupdate();
}

static void draw_activated(void)
{
    dclear(C_WHITE);
    info_title(L("Activation", "Ativacao", "Activation"), COLOR);

    /* A green disc with a thick white tick */
    int const cx = DWIDTH / 2, cy = 70;
    dcircle(cx, cy, 34, GOOD, GOOD);
    for (int t = -3; t <= 3; t++) {
        dline(cx - 17, cy + t, cx - 5, cy + 12 + t, C_WHITE);
        dline(cx - 5, cy + 12 + t, cx + 18, cy - 13 + t, C_WHITE);
    }

    font_t const *old = dfont(&font_big);
    dtext_opt(cx, 120, GOOD, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Activated", "Ativado", "Activee"));
    dfont(old);
    dtext_opt(cx, 144, C_BLACK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("This calculator is activated. Thank you!",
          "Esta calculadora esta ativada. Obrigado!",
          "Cette calculatrice est activee. Merci!"));
    dprint_opt(cx, 160, C_DARK, C_NONE, DTEXT_CENTER, DTEXT_TOP,
        L("Request code %s, activation code %s",
          "Pedido %s, ativacao %s", "Demande %s, activation %s"),
        request, expected);

    fkey_button(1, L("RESET", "REPOR", "RAZ"));
    fkey_button(6, L("ABOUT", "SOBRE", "INFOS"));
    dupdate();
}

int main(void)
{
    lang_init();
    usb_screen_init();
    make_codes();

    bool active = saved_ok(), wrong = false;
    char typed[CODE_LEN + 1] = { 0 };

    while (1) {
        if (active) draw_activated();
        else draw_request(typed, wrong);

        int key = getkey().key;
        int n = strlen(typed);
        if (key == KEY_EXIT) return 1;
        if (key == INFO_KEY || key == KEY_F6) {
            info_show("Activation", L("Activation " APPS_VERSION,
                "Ativacao " APPS_VERSION, "Activation " APPS_VERSION), COLOR);
        }
        else if (active) {
            if (key == KEY_F1) { /* back to the request screen */
                remove(SAVE_PATH);
                active = false;
                typed[0] = 0;
            }
        }
        else if (key_hex(key) >= 0 && n < CODE_LEN) {
            typed[n] = "0123456789ABCDEF"[key_hex(key)];
            typed[n + 1] = 0;
            wrong = false;
        }
        else if (key == KEY_DEL && n > 0) {
            typed[n - 1] = 0;
            wrong = false;
        }
        else if (key == KEY_F1 || key == KEY_ACON) {
            typed[0] = 0;
            wrong = false;
        }
        else if (key == KEY_EXE && n == CODE_LEN) {
            if (!strcmp(typed, expected)) {
                save(typed);
                active = true;
            }
            else wrong = true;
        }
    }
}
