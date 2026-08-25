/* ---------------------------------------------------------------------------
 * x48_shim.h - the ONLY surface of the x48 C engine that Agape48's C++ sees.
 *
 * Why a shim at all: x48 (Dost), x48ng (Le Moine) and Droid48 each expose a
 * different set of globals and entry points, and all three reach into a shared
 * `saturn` struct from their UI code. Adapting each fork behind these ~16
 * functions means a vendor swap is a rewrite of x48_shim.c and nothing else -
 * never the bridge, never the QML.
 *
 * Threading contract: every function here must be called from one thread only.
 * Agape48 calls them from the Qt GUI thread (see Agape48Engine).
 * ------------------------------------------------------------------------- */
#ifndef AGAPE48_X48_SHIM_H
#define AGAPE48_X48_SHIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The HP 48 LCD is 131x64 visible. The Saturn display driver can offset the
 * scanline start, so the frame buffer is over-wide and the visible window is
 * reported per frame. */
#define X48_LCD_WIDTH       131
#define X48_LCD_HEIGHT       64
#define X48_LCD_STRIDE      144
#define X48_LCD_PIXELS      (X48_LCD_STRIDE * X48_LCD_HEIGHT)

/* Keyboard matrix: 9 "out" rows driven by the Saturn, up to 8 "in" columns
 * read back. The ON key is not in the matrix - x48 parks it in row 8.
 * TODO(vendor): confirm the ON row index against the fork you vendored. */
#define X48_KB_ROWS           9
#define X48_KB_ROW_ON         8

/* Annunciator bits, in the left-to-right order they appear on the glass. */
#define X48_ANN_LEFT     0x0001u   /* left shift  */
#define X48_ANN_RIGHT    0x0002u   /* right shift */
#define X48_ANN_ALPHA    0x0004u
#define X48_ANN_BATTERY  0x0008u
#define X48_ANN_BUSY     0x0010u
#define X48_ANN_IO       0x0020u

typedef struct {
    const char *rom_path;     /* required; NULL means "look next to state" */

    /* Desktop: a directory holding ram / port1 / port2 / state.
     * Android SAF: leave NULL and hand over pre-opened descriptors instead,
     * because a content:// tree URI has no POSIX path to give. */
    const char *state_dir;
    int   fd_ram;             /* -1 when unused */
    int   fd_port1;
    int   fd_port2;
    int   fd_state;

    bool  read_only;          /* mount the card ports read-only */
    bool  throttle;           /* pace to real HP 48 speed vs. run free */
} x48_config_t;

typedef struct {
    /* One byte per pixel, 0 or 1. Indexed8 rather than packed bits: 8.4 KB is
     * nothing, and it lets QImage wrap the buffer with no bit twiddling. */
    uint8_t  pixels[X48_LCD_PIXELS];
    int      width;           /* visible width, normally X48_LCD_WIDTH */
    int      height;          /* visible height; 0 when the LCD is off */
    int      stride;
    int      contrast;        /* 0..31 as programmed by the Saturn */
    uint16_t annunciators;    /* X48_ANN_* bitfield */
} x48_frame_t;

/* --- lifecycle ---------------------------------------------------------- */

/* Loads ROM + state and brings the Saturn up. Returns false and sets
 * x48_last_error() on a bad or missing ROM. */
bool        x48_init(const x48_config_t *cfg);
void        x48_shutdown(void);

/* Runs up to max_cycles Saturn cycles, returns the number actually executed.
 * The HP 48 clock is ~4 MHz / ~2 MHz depending on model, so a 60 Hz tick wants
 * roughly 70000 cycles. Returns 0 if the CPU is halted in deep sleep. */
int         x48_run_slice(int max_cycles);

/* True if the CPU is in SHUTDN and nothing but a key or timer will wake it -
 * the frontend can then stop ticking and let the device sleep. */
bool        x48_is_asleep(void);

/* --- display ------------------------------------------------------------ */

/* Copies the current LCD into *out. Returns false if nothing changed since the
 * previous call, so the caller can skip the texture upload entirely. */
bool        x48_take_frame(x48_frame_t *out);

/* --- keyboard ----------------------------------------------------------- */

/* row is 0..X48_KB_ROWS-1, mask is the "in" column bit. Multiple simultaneous
 * presses are the point: ON+A+F is the HP 48 hard reset and must arrive as
 * three live keys, not a sequence. */
void        x48_key_down(int row, uint16_t mask);
void        x48_key_up(int row, uint16_t mask);
void        x48_key_release_all(void);

/* --- state -------------------------------------------------------------- */

void        x48_reset(bool cold);        /* cold == wipe RAM (ON+A+F equivalent) */
bool        x48_save_state(void);
bool        x48_reload_state(void);      /* re-read after an external sync wrote it */

/* Fingerprint of the on-disk state, for detecting "another device wrote this".
 * Cheap: size + mtime + a hash of the first and last page. */
uint64_t    x48_state_fingerprint(void);

/* --- clipboard ---------------------------------------------------------- */

/* Renders level 1 of the RPL stack as UTF-8 into buf. Returns the byte length
 * written, or the required length (> buflen) if buf was too small. */
size_t      x48_stack_to_text(char *buf, size_t buflen);

/* Parses UTF-8 and pushes the result onto the stack. Returns false if the text
 * is not a valid RPL object. */
bool        x48_text_to_stack(const char *utf8);

/* --- beeper ------------------------------------------------------------- */

/* Non-zero return means the Saturn asked for a beep since the last call; the
 * frontend plays it. Consumes the request. */
bool        x48_take_beep(uint32_t *freq_hz, uint32_t *duration_ms);

/* --- diagnostics -------------------------------------------------------- */

const char *x48_last_error(void);
const char *x48_core_version(void);      /* which fork/revision got vendored */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* AGAPE48_X48_SHIM_H */
