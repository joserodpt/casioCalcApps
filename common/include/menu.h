//---
//	menu - The numbered tool list that toolkit apps open on
//
//	static menu_item_t const items[] = {
//	    { "Unit converter", "Conversor de unidades",
//	      "Length, volume, mass...", "Comprimento, volume...", units_run,
//	      COLOR },
//	    ...
//	};
//	static menu_t const menu = { "Physics", "Physics toolkit " APPS_VERSION,
//	    "Ferramentas de fisica " APPS_VERSION, COLOR, items, 6 };
//	menu_run(&menu);
//
//	UP/DOWN select, EXE/RIGHT or 1-7 open a tool, EXIT quits.
//---

#ifndef CASIO_MENU
#define CASIO_MENU

#define MENU_MAX 7

typedef struct {
    char const *name_en, *name_pt;
    char const *desc_en, *desc_pt;      /* one line under the name */
    void (*run)(void);
    int color;
} menu_item_t;

typedef struct {
    char const *app;                    /* title bar and info screen */
    char const *about_en, *about_pt;    /* subtitle on the info screen */
    int color;
    menu_item_t const *items;
    int count;                          /* at most MENU_MAX */
} menu_t;

/* menu_run(): Show the menu until EXIT; also sets app_name (info.h) */
void menu_run(menu_t const *m);

#endif /* CASIO_MENU */
