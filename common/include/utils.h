//---
//	utils - F-key buttons and a scrollbar
//---

#ifndef CASIO_UTILS
#define CASIO_UTILS

/* scrollbar_px(): Pixel-based scrollbar at the right edge of the screen
   @view_top     First pixel covered by scrollbar area
   @view_bottom  Pixel below last pixel covered by scrollbar area
   @range_min    Minimum value in the virtual range we're scrolling through
   @range_max    Maximum value in the virtual range
   @range_pos    Position within the virtual range
   @range_view   How much of the range is visible on-screen */
void scrollbar_px(int view_top, int view_bottom, int range_min, int range_max,
	int range_pos, int range_view);

/* fkey_button(): A rectangular F-key (position 1..6) */
void fkey_button(int position, char const *text);

/* fkey_button_color(): A rectangular F-key with a custom background */
void fkey_button_color(int position, char const *text, int color);

#endif /* CASIO_UTILS */
