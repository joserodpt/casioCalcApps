//---
//	usb_screen - Let a PC see the screen over USB
//
//	Call usb_screen_init() once at start-up, after lang_init() (a world
//	switch closes the USB link). It opens gint's fxlink USB interface and
//	sends every frame dupdate() shows, so emuCasioCalc (File > Show
//	calculator's screen) or `fxlink -i` can show the app live.
//
//	It never makes the app wait for the PC: a frame is sent only once the
//	last one has gone, else it is skipped, and with the cable plugged into a
//	PC that doesn't read, the app loses at most ~0.2 s once, then nothing.
//	Going to the main menu (MENU) closes the link (gint reopens it on the
//	way back, but the PC may not reconnect until the app starts again).
//---

#ifndef CASIO_USB_SCREEN
#define CASIO_USB_SCREEN

/* usb_screen_init(): Open the USB link and send each frame shown. */
void usb_screen_init(void);

#endif /* CASIO_USB_SCREEN */
