/* ---------------------------------------------------------------------------
 * x48_shim.c - adapter between x48_shim.h and the vendored x48 fork.
 *
 * STATUS: skeleton. Every body below is either a no-op or a stub that fails
 * loudly. Nothing here silently pretends to work: an unvendored build starts,
 * shows the skin, and reports "core not vendored" instead of a black LCD you
 * then have to debug.
 *
 * Filling this in is step 1 of VENDORING.md.
 * ------------------------------------------------------------------------- */
#include "x48_shim.h"

#include <string.h>

/* When the x48 sources are added to src/core/CMakeLists.txt, define this and
 * the real bodies below get compiled instead of the stubs. */
#ifdef AGAPE48_X48_VENDORED
#  include "hp48.h"       /* x48: saturn_t, saturn */
#  include "hp48_emu.h"   /* x48: emulate(), reset_saturn(), ... */
#endif

static char        s_error[256] = "x48 core not vendored - see src/core/VENDORING.md";
static x48_frame_t s_shadow;        /* last frame handed out, for change detection */
static bool        s_have_shadow;

static void set_error(const char *msg)
{
    strncpy(s_error, msg, sizeof(s_error) - 1);
    s_error[sizeof(s_error) - 1] = '\0';
}

/* --- lifecycle ---------------------------------------------------------- */

bool x48_init(const x48_config_t *cfg)
{
    (void)cfg;
#ifdef AGAPE48_X48_VENDORED
    /* TODO(vendor):
     *   1. point x48's `homedir`/`rom_path` globals at cfg->state_dir /
     *      cfg->rom_path, or - on Android - at the fds in cfg->fd_*, which
     *      means replacing romio.c's fopen() calls with fdopen().
     *   2. call x48's init_emulator() / read_files().
     *   3. mirror x48's own error string into set_error(). */
    return false;
#else
    set_error("x48 core not vendored - see src/core/VENDORING.md");
    return false;
#endif
}

void x48_shutdown(void)
{
    s_have_shadow = false;
}

int x48_run_slice(int max_cycles)
{
    (void)max_cycles;
    /* TODO(vendor): x48's emulate() is an unbounded loop driven by its own
     * X11 event pump. The port has to invert that: expose a cycle-budgeted
     * step, i.e. `while (cycles < budget) cycles += step_instruction();`.
     * x48ng already has step_instruction() factored out; original x48 does
     * not and needs emulate() split. This is the single biggest piece of
     * upstream surgery in the whole project. */
    return 0;
}

bool x48_is_asleep(void)
{
    return true;   /* stub: never asks to be ticked */
}

/* --- display ------------------------------------------------------------ */

bool x48_take_frame(x48_frame_t *out)
{
    if (!out)
        return false;

    /* TODO(vendor): read x48's display RAM (saturn.display + the annunciator
     * latch) and expand it to one byte per pixel here. Compare against
     * s_shadow and return false when identical, so LcdItem can skip the
     * texture upload on the many frames where the HP 48 draws nothing. */
    if (!s_have_shadow) {
        memset(&s_shadow, 0, sizeof(s_shadow));
        s_shadow.width  = X48_LCD_WIDTH;
        s_shadow.height = X48_LCD_HEIGHT;
        s_shadow.stride = X48_LCD_STRIDE;
        s_have_shadow   = true;
        *out = s_shadow;
        return true;      /* one frame, so the LCD area paints its background */
    }
    return false;
}

/* --- keyboard ----------------------------------------------------------- */

void x48_key_down(int row, uint16_t mask)
{
    (void)row; (void)mask;
    /* TODO(vendor): saturn.keybuf.rows[row] |= mask; then do_kbd_int() so the
     * Saturn wakes from SHUTDN. Do NOT clear other rows - simultaneous keys
     * are required for ON+A+F. */
}

void x48_key_up(int row, uint16_t mask)
{
    (void)row; (void)mask;
    /* TODO(vendor): saturn.keybuf.rows[row] &= ~mask; */
}

void x48_key_release_all(void)
{
    /* TODO(vendor): memset(saturn.keybuf.rows, 0, ...).
     * Called on window deactivation and on touch cancel, so a key does not
     * stay stuck down when the app loses focus mid-press. */
}

/* --- state -------------------------------------------------------------- */

void x48_reset(bool cold)  { (void)cold; }
bool x48_save_state(void)  { set_error("core not vendored"); return false; }
bool x48_reload_state(void){ set_error("core not vendored"); return false; }

uint64_t x48_state_fingerprint(void) { return 0; }

/* --- clipboard ---------------------------------------------------------- */

size_t x48_stack_to_text(char *buf, size_t buflen)
{
    /* TODO(vendor): x48 has no stack decompiler of its own. Two options, both
     * in README "Clipboard": (a) drive the HP 48 itself - put the object on
     * the stack, run ->STR via a synthetic keypress sequence, read the result
     * string out of RAM; (b) port a subset of x48's rpl.c / an RPL object
     * walker. (a) is ~50 lines and always correct; (b) is faster but is a
     * decompiler you now maintain. */
    if (buf && buflen) buf[0] = '\0';
    return 0;
}

bool x48_text_to_stack(const char *utf8)
{
    (void)utf8;
    /* TODO(vendor): the inverse - stuff the text into a string object in RAM
     * and run STR-> , or type it in as keystrokes for short payloads. */
    return false;
}

/* --- beeper ------------------------------------------------------------- */

bool x48_take_beep(uint32_t *freq_hz, uint32_t *duration_ms)
{
    (void)freq_hz; (void)duration_ms;
    /* TODO(vendor): x48 traps the BEEP entry point and/or the OUT register
     * toggling. Capture (frequency, duration) and hand it up once. */
    return false;
}

/* --- diagnostics -------------------------------------------------------- */

const char *x48_last_error(void)   { return s_error; }
const char *x48_core_version(void) { return "stub (no core vendored)"; }
