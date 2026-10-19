#include <gint/display.h>
#include <gint/usb.h>
#include <gint/usb-ff-bulk.h>
#include <gint/defs/timeout.h>
#include <gint/cpu.h>

#include "usb_screen.h"

/* How long one frame may hold the app up, per step (the pixels, then the
   commit), before the rest is left to finish in the background */
#define STEP_MS  100

/* One fxlink "video" message: header, image subheader, then the pixels
   (from VRAM, still on screen). Static: a transfer that outlives
   send_frame() may still read them */
static usb_fxlink_header_t header;
static usb_fxlink_image_t subheader;

/* Set by the USB interrupt when a transfer ends. Static for the same
   reason: a late callback must not write into a returned stack frame */
static int volatile pixels_done, commit_done;

/* Where the last frame got to */
static enum { IDLE, SENDING_PIXELS, COMMITTING } stage = IDLE;

/* Waits for *flag, up to STEP_MS */
static bool wait_for(int volatile *flag)
{
    timeout_t t = timeout_make_ms(STEP_MS);
    while (!*flag) {
        if (timeout_elapsed(&t)) return false;
        sleep();
    }
    return true;
}

/* Moves the last frame on, without waiting. True once it is all sent */
static bool previous_done(int pipe)
{
    if (stage == SENDING_PIXELS) {
        if (!pixels_done) return false;
        commit_done = 0;
        if (usb_commit_async(pipe, GINT_CALL_SET(&commit_done)) < 0)
            return false;
        stage = COMMITTING;
    }
    if (stage == COMMITTING) {
        if (!commit_done) return false;
        stage = IDLE;
    }
    return true;
}

/* The dupdate() hook: runs right after each frame is shown */
static void send_frame(void)
{
    if (!usb_is_open_interface(&usb_ff_bulk)) {
        stage = IDLE;   /* the link was reset: nothing is in flight */
        return;
    }
    int pipe = usb_ff_bulk_output();
    if (!previous_done(pipe)) return;   /* still going: skip this frame */

    int size = DWIDTH * DHEIGHT * 2;
    usb_fxlink_fill_header(&header, "fxlink", "video",
        size + sizeof subheader);
    subheader.width = htole32(DWIDTH);
    subheader.height = htole32(DHEIGHT);
    subheader.pixel_format = htole32(USB_FXLINK_IMAGE_RGB565);

    /* The headers fit the pipe's buffer: these return at once */
    if (usb_write_async(pipe, &header, sizeof header, false,
        GINT_CALL_NULL) < 0) return;
    if (usb_write_async(pipe, &subheader, sizeof subheader, false,
        GINT_CALL_NULL) < 0) return;

    pixels_done = 0;
    if (usb_write_async(pipe, gint_vram, size, false,
        GINT_CALL_SET(&pixels_done)) < 0) return;
    stage = SENDING_PIXELS;

    /* Usually done in a few ms; if the PC isn't reading, the next frames
       finish it (or skip) */
    if (wait_for(&pixels_done) && previous_done(pipe) && wait_for(&commit_done))
        previous_done(pipe);
}

void usb_screen_init(void)
{
    static usb_interface_t const *intf[] = { &usb_ff_bulk, NULL };
    int rc = usb_open(intf, GINT_CALL_NULL);
    if (rc < 0 && rc != USB_OPEN_ALREADY_OPEN) return;
    dupdate_set_hook(GINT_CALL(send_frame));
}
