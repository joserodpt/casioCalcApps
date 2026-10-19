#include <gint/gint.h>
#include <gint/config.h>
#include <string.h>

#include "i18n.h"

int lang_current = L_EN;

#if GINT_HW_FX

/* The fx-9860G OS has other syscalls (and no LocalizeMessage1 as on the
   fx-CG): its apps stay in English */
void lang_init(void)
{
}

#else

/* A syscall stub: jump to the OS's syscall entry with the number in r0.
   (Same trick the syscall test apps use.) The symbol is weak so a test app
   that defines its own stub of the same name still links. */
#define SYSCALL(name, number) \
    __asm__(".text\n.weak _" #name "\n.align 1\n_" #name ":\n" \
        "\tmov.l 1f, r2\n\tmov.l 2f, r0\n\tjmp @r2\n\tnop\n" \
        ".align 2\n1:\t.long 0x80020070\n2:\t.long " #number "\n")

/* LocalizeMessage1(msgno, result): OS message `msgno` into `result`.
   Message 1 is the current language's own name. */
SYSCALL(LocalizeMessage1, 0x12FC);
int LocalizeMessage1(int msgno, char *result);

/* Identify the language from its name (first three letters). Casio's
   encoding for accented letters is not documented, so only the ASCII
   prefix is compared; Chinese starts with a non-ASCII byte. */
static int os_language(void)
{
    char name[88] = "";
    LocalizeMessage1(1, name);

    if ((unsigned char)name[0] >= 0x80) return L_ZH;    /* GB 18030 */
    if (!strncmp(name, "Esp", 3)) return L_ES;
    if (!strncmp(name, "Deu", 3)) return L_DE;
    if (!strncmp(name, "Fra", 3)) return L_FR;
    if (!strncmp(name, "Por", 3)) return L_PT;
    return L_EN;    /* English, or a language added with a .g3l file */
}

/* Runs in OS mode (via the world switch), where OS syscalls are callable */
static int detect(void)
{
    lang_current = os_language();
    return 0;
}

void lang_init(void)
{
    gint_world_switch(GINT_CALL(detect));
}

#endif /* GINT_HW_FX */
