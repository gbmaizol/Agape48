/* ---------------------------------------------------------------------------
 * x48_shim.c - adapter between Agape48's C++ and the vendored Droid48 fork.
 *
 * Vendored core: Droid48's app/src/main/jni tree (decision 1, 2026aug28).
 * That tree is x48 (Dost 1994) with the X11 frontend removed and a JNI one
 * bolted on. We compile neither frontend: main.c and x48.c stay out of the
 * build, and the symbols they used to define are provided here instead. That
 * linkage gap is the whole integration - see "The sleep hook" below.
 * ------------------------------------------------------------------------- */

#include "x48_shim.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* The vendored tree's headers. Order matters: hp48.h wants global.h first. */
#include "x48/global.h"
#include "x48/hp48.h"
#include "x48/hp48_emu.h"
#include "x48/x48.h"
#include "x48/romio.h"
#include "x48/device.h"   /* ANN_* */
#include "x48/rpl.h"     /* DSKTOP, TEMPTOP, the DO* prologues */

/* Defined in the vendored lcd.c. Not declared in any header there, because the
 * X11 frontend reached straight into the file. 64 rows of nibble values. */
extern unsigned char lcd_buffer[64][NIBBLES_PER_ROW + 2];

/* Appended to the vendored emulate.c by Agape48. */
extern void agape48_emulate_begin(void);
extern int  agape48_emulate_slice(int max_instructions);

/* Appended to the vendored init.c by Agape48: init_emulator() without the
 * exit(1). Returns NULL on success, otherwise a reason to show the user. */
extern const char *agape48_init_emulator(void);

/* ---------------------------------------------------------------------------
 * Symbols the vendored tree expects from a frontend.
 *
 * romio.c and init.c read these paths; main.c used to define them.
 * ------------------------------------------------------------------------ */
char files_path[256];
char rom_filename[256];
char ram_filename[256];
char conf_filename[256];
char port1_filename[256];
char port2_filename[256];

int  exit_state;      /* main.c owned this; actions.c owns got_alarm */
extern int got_alarm;

/* main.c owned the machine itself, and the program name it logged under. */
saturn_t saturn;
char    *progname = "agape48";

/* disasm.c is not built; options.c still reads this. 0 is HP mnemonics. */
int disassembler_mode;

/* debugger.c is not built; the core still branches on these. Both stay 0, so
 * every "are we in the debugger" test in the vendored tree answers no. */
int enter_debugger;
int in_debugger;

/* ---------------------------------------------------------------------------
 * X11 leftovers.
 *
 * Droid48 replaced Xlib with the dummy structs in x48.h but left the call sites
 * in place, so lcd.c and actions.c still reference a Display and still call
 * four Xlib functions that now do nothing. Rather than edit those call sites -
 * dozens of them, in the files most likely to be re-vendored - the seam absorbs
 * them here. This is the shim earning its keep.
 * ------------------------------------------------------------------------ */
disp_t   disp;
Display *dpy;

void adjust_contrast(int contrast)          { (void)contrast; }
void ShowConnections(char *w, char *i)      { (void)w; (void)i; }
void XClearWindow(Display *d, Window w)     { (void)d; (void)w; }

void XClearArea(Display *d, Window w, int x, int y, int width, int height, int boo)
{
    (void)d; (void)w; (void)x; (void)y; (void)width; (void)height; (void)boo;
}

Pixmap XCreateBitmapFromData(Display *d, Window w, char *data, int a, int b)
{
    (void)d; (void)w; (void)data; (void)a; (void)b;
    Pixmap p;
    memset(&p, 0, sizeof p);
    return p;
}

/* ---------------------------------------------------------------------------
 * The sleep hook.
 *
 * do_shutdown() in actions.c parks the calculator in a
 *     do { blockConditionVariable(); ... } while (wake == 0 && exit_state);
 * loop. Under Droid48 that blocks a dedicated emulator thread on a pthread
 * condition variable until the Java side fires an alarm. Agape48 emulates on
 * the Qt GUI thread, where blocking is not an option - the UI would freeze for
 * as long as the calculator sleeps, which is most of the time.
 *
 * The escape is already in that while condition. Clearing exit_state makes
 * do_shutdown run exactly one wake-check pass and return. So this hook does not
 * block at all: it lets one pass happen, then hands the GUI thread back. The
 * frontend drops to the slow idle tick and each tick re-enters, runs one more
 * pass, and either wakes the machine or sleeps again.
 *
 * The catch, and it cost dogfood #11 through #13: do_shutdown returns whether
 * or not anything woke, because `exit_state == 0` ends its loop by itself. The
 * Saturn's PC is already past the SHUTDN by then, so the ROM carries straight
 * on as though it had been woken - a wake that never happened. Once per key
 * press that is invisible; ten times a second it is not, and an HP 48 that has
 * been switched off decides after a few hundred milliseconds that it is awake
 * and paints the stack back on the screen.
 *
 * So the PC goes back on the SHUTDN whenever the pass found no reason to wake,
 * and the machine re-enters it on the next tick. x48_shutdn_woke is how
 * actions.c reports which of the two happened; that one line is the only edit
 * in the vendored file.
 * ------------------------------------------------------------------------ */
static void detect_rom_revision(void);   /* defined with the object interchange code */

static bool s_asleep;
static long s_shutdn_pc;      /* PC just after the SHUTDN we are parked on */

int x48_shutdn_woke;          /* set by do_shutdown(), read below */
int x48_parked;               /* read by agape48_emulate_slice() */

void blockConditionVariable(void)
{
    s_asleep    = true;
    s_shutdn_pc = saturn.PC;
    got_alarm   = 1;   /* let the timer catch-up body run this pass */
    exit_state  = 0;   /* ...then leave, rather than waiting for a wake */
}

/* ---------------------------------------------------------------------------
 * Serial.
 *
 * serial.c is not compiled - see the note in CMakeLists.txt. Droid48 does not
 * have serial either, so this changes no behaviour; it is also what lets the
 * tree build for Windows. receive_char() must leave interrupt_called alone or
 * do_shutdown (actions.c:687) would read a wake that never happened, which is
 * exactly what the real one does with wire_fd == -1.
 * ------------------------------------------------------------------------ */
int  serial_init(void)     { return 0; }
void serial_baud(int baud) { (void)baud; }
void transmit_char(void)   { }
void receive_char(void)    { }

/* The vendored tree's event pump - X11 in x48.c, JNI in main.c. Agape48 has no
 * event loop of its own, so x48_key_down() queues and this drains.
 *
 * The interrupt must be raised from inside GetEvent(), not from x48_key_down():
 * do_shutdown (actions.c:651) zeroes interrupt_called, calls GetEvent(), and
 * wakes only if do_kbd_int() set that flag during the call. Raising it earlier
 * is invisible to the sleep loop - which is why ON could not wake a sleeping
 * machine while this returned a constant 0 - and raising it in both places
 * would push the return address twice. So this is the single delivery point,
 * pumped once per slice by agape48_emulate_slice and again by do_shutdown. */
static int s_key_pending;

int GetEvent(void)
{
    if (!s_key_pending)
        return 0;
    s_key_pending = 0;
    do_kbd_int();
    return 1;
}

/* ------------------------------------------------------------------------ */

static char        s_error[256] = "";
static bool        s_ready;
static bool        s_dirty = true;   /* force the first frame out */

static void set_error(const char *msg)
{
    snprintf(s_error, sizeof s_error, "%s", msg);
}

static void join_path(char *dst, size_t n, const char *dir, const char *leaf)
{
    if (dir && *dir)
        snprintf(dst, n, "%s/%s", dir, leaf);
    else
        snprintf(dst, n, "%s", leaf);
}

/* --- lifecycle ---------------------------------------------------------- */

bool x48_init(const x48_config_t *cfg)
{
    if (!cfg) {
        set_error("x48_init: no config");
        return false;
    }

    /* Android SAF hands over descriptors rather than paths. Item 4 settled
     * this as copy-in/copy-out, so by the time we get here StateFileManager
     * has already staged the files somewhere with a real POSIX path and the
     * fd_* fields are unused. Refuse rather than silently emulating nothing. */
    if (!cfg->state_dir || !*cfg->state_dir) {
        set_error("x48_init: state_dir is required "
                  "(SAF is copy-in/copy-out, see design-questions item 4)");
        return false;
    }

    /* The vendored init.c builds every path as strcpy(files_path) followed by
     * strcat(leaf) - a bare concatenation with no separator inserted. So
     * files_path carries the trailing slash and the rest are bare leaf names.
     * Getting this wrong yields "/state/dir/state/dir/rom" and a fatal exit. */
    snprintf(files_path, sizeof files_path, "%s/", cfg->state_dir);
    snprintf(ram_filename,   sizeof ram_filename,   "%s", "ram");
    snprintf(conf_filename,  sizeof conf_filename,  "%s", "hp48");
    snprintf(port1_filename, sizeof port1_filename, "%s", "port1");
    snprintf(port2_filename, sizeof port2_filename, "%s", "port2");

    /* The ROM is bundled (decision 7), so it sits outside the state directory.
     * init.c now takes an absolute rom_filename verbatim and never writes it
     * back. A NULL rom_path keeps the old meaning: a leaf beside the state. */
    if (cfg->rom_path && *cfg->rom_path) {
        if (strlen(cfg->rom_path) >= sizeof rom_filename) {
            set_error("x48_init: rom_path is longer than the core's 256-byte "
                      "rom_filename");
            return false;
        }
        strcpy(rom_filename, cfg->rom_path);
    } else {
        snprintf(rom_filename, sizeof rom_filename, "%s", "rom");
    }

    /* Probe the ROM first, purely so the message can name the path. The core
     * distinguishes "not a ROM" from "out of memory" but not from "not there",
     * and the path is the useful half of a missing-ROM report. */
    {
        char probe[1024];
        FILE *f;
        /* Must agree with agape48_rom_is_absolute() in init.c, which already
         * knows about drive letters. This probe did not, so on Windows an
         * absolute ROM path fell into the concatenation branch and produced
         * "C:/state/dir/C:/rom/path" - the probe failed before init.c's correct
         * logic ever ran, and x48_init reported a ROM it could not read while
         * naming a path nobody had asked for. */
        if (rom_filename[0] == '/' || rom_filename[0] == '\\'
#ifdef _WIN32
            || (rom_filename[0] != '\0' && rom_filename[1] == ':')
#endif
           )
            snprintf(probe, sizeof probe, "%s", rom_filename);
        else
            snprintf(probe, sizeof probe, "%s%s", files_path, rom_filename);
        if (NULL == (f = fopen(probe, "rb"))) {
            snprintf(s_error, sizeof s_error, "x48_init: cannot read ROM %s",
                     probe);
            return false;
        }
        fclose(f);
    }

    exit_state = 1;
    s_asleep   = false;

    /* init_emulator() cannot be used from a GUI: it returns 0 for success, has
     * no failure return, and reached exit(1) on a bad ROM. agape48_init_emulator
     * replaces it in the vendored init.c and returns NULL or a reason. */
    {
        const char *why = agape48_init_emulator();
        if (why) {
            snprintf(s_error, sizeof s_error, "x48_init: %s", why);
            return false;
        }
    }
    init_active_stuff();
    agape48_emulate_begin();

    s_ready = true;
    detect_rom_revision();   /* for the HPHP48- transfer header */
    s_dirty = true;
    set_error("");
    return true;
}

void x48_shutdown(void)
{
    if (!s_ready)
        return;
    write_files();
    s_ready = false;
}

int x48_run_slice(int max_cycles)
{
    if (!s_ready)
        return 0;

    /* The vendored loop budgets by instruction, not by Saturn cycle: its unit
     * is one step_instruction(). The header's "cycles" is therefore read as
     * instructions here. At ~4 MHz and ~4 cycles per instruction, a 60 Hz tick
     * wants roughly 17000 of these rather than the 70000 the header suggests. */
    exit_state = 1;
    int n = agape48_emulate_slice(max_cycles);

    /* blockConditionVariable() clears exit_state when the machine parks in
     * SHUTDN. Restore it so the next slice can run, and report the state. */
    /* Woken by its own SHUTDN pass, with budget to spare: carry on rather than
     * making the machine wait for the next tick to act on the key. */
    if (!exit_state && x48_shutdn_woke && n < max_cycles) {
        exit_state = 1;
        n += agape48_emulate_slice(max_cycles - n);
    }

    if (!exit_state) {
        s_asleep = true;
        x48_parked = !x48_shutdn_woke;
        if (x48_parked)
            saturn.PC = s_shutdn_pc - 3;   /* re-execute SHUTDN, see above */
    } else {
        s_asleep = false;
        x48_parked = 0;
    }
    exit_state = 1;

    return n;
}

bool x48_is_asleep(void)
{
    return s_asleep;
}

/* --- display ------------------------------------------------------------ */

bool x48_take_frame(x48_frame_t *out)
{
    if (!out || !s_ready)
        return false;

    /* draw_nibble() in the vendored lcd.c writes lcd_buffer only when a nibble
     * actually changes, so the buffer is authoritative but carries no dirty
     * flag of its own. Comparing against our own shadow is cheap - 2.3 KB - and
     * it means a frame is skipped whenever the HP 48 drew nothing, which is
     * most frames. That is what makes the 60 Hz redraw affordable. */
    static unsigned char shadow[64][NIBBLES_PER_ROW + 2];
    static int           shadow_on = -1;
    static int           shadow_ann = -1;
    static int           shadow_contrast = -1;

    /* display.on has to be part of this. Switching the LCD off changes no
     * nibble in lcd_buffer, so a buffer-only comparison reported "nothing
     * drew" and the frontend went on showing the last frame of a calculator
     * that is no longer displaying anything. Same for the annunciators, which
     * live outside the buffer entirely. */
    bool changed = s_dirty
                || display.on      != shadow_on
                || display.annunc  != shadow_ann
                || display.contrast != shadow_contrast
                || memcmp(shadow, lcd_buffer, sizeof shadow) != 0;
    if (!changed)
        return false;

    memcpy(shadow, lcd_buffer, sizeof shadow);
    shadow_on       = display.on;
    shadow_ann      = display.annunc;
    shadow_contrast = display.contrast;
    s_dirty = false;

    memset(out->pixels, 0, sizeof out->pixels);

    out->stride       = X48_LCD_STRIDE;
    out->width        = X48_LCD_WIDTH;
    out->height       = display.on ? X48_LCD_HEIGHT : 0;
    out->contrast     = display.contrast;
    out->annunciators = 0;

    /* ANN_* are not single bits: every one carries 0x80 as well as its own
     * flag, so a plain & is true for all of them whenever any is set. The
     * vendored draw_annunc() tests for equality and so must this. */
    const int ann = display.annunc;
    #define A48_ANN(m) (((ann) & (m)) == (m))
    if (A48_ANN(ANN_LEFT))    out->annunciators |= X48_ANN_LEFT;
    if (A48_ANN(ANN_RIGHT))   out->annunciators |= X48_ANN_RIGHT;
    if (A48_ANN(ANN_ALPHA))   out->annunciators |= X48_ANN_ALPHA;
    if (A48_ANN(ANN_BATTERY)) out->annunciators |= X48_ANN_BATTERY;
    if (A48_ANN(ANN_BUSY))    out->annunciators |= X48_ANN_BUSY;
    if (A48_ANN(ANN_IO))      out->annunciators |= X48_ANN_IO;
    #undef A48_ANN

    if (!display.on)
        return true;

    /* One nibble is four horizontal pixels, least significant bit leftmost. */
    for (int row = 0; row < X48_LCD_HEIGHT; row++) {
        uint8_t *dst = out->pixels + (size_t)row * X48_LCD_STRIDE;
        for (int col = 0; col < NIBBLES_PER_ROW; col++) {
            unsigned char v = lcd_buffer[row][col] & 0x0f;
            int x = col * 4;
            for (int bit = 0; bit < 4 && x + bit < X48_LCD_WIDTH; bit++)
                dst[x + bit] = (v >> bit) & 1;
        }
    }

    return true;
}

/* --- keyboard ----------------------------------------------------------- */

/* ON carries X48_KB_MASK_ON and sets that bit in every one of the nine rows,
 * exactly as x48.c:381 does; the row argument is ignored for it. Every other
 * key is one row and one bit. */
void x48_key_down(int row, uint16_t mask)
{
    short before;
    int   i;

    if (!s_ready)
        return;

    if (mask & X48_KB_MASK_ON) {
        for (i = 0; i < X48_KB_ROWS; i++)
            saturn.keybuf.rows[i] |= (short)X48_KB_MASK_ON;
        s_key_pending = 1;     /* ON always raises, unconditionally */
        s_asleep = false;
        return;
    }

    if (row < 0 || row >= X48_KB_ROWS)
        return;

    before = saturn.keybuf.rows[row];
    saturn.keybuf.rows[row] |= (short)mask;

    /* x48.c:388 - an ordinary key raises the interrupt only on a fresh press,
     * and only while the ROM has keyboard interrupts enabled. The raise itself
     * happens in GetEvent(), see the note there. */
    if (saturn.kbd_ien && (short)(before & mask) != (short)mask)
        s_key_pending = 1;

    s_asleep = false;      /* a keypress is a wake reason */
}

void x48_key_up(int row, uint16_t mask)
{
    int i;

    if (!s_ready)
        return;

    if (mask & X48_KB_MASK_ON) {
        for (i = 0; i < X48_KB_ROWS; i++)
            saturn.keybuf.rows[i] &= (short)~X48_KB_MASK_ON;
        return;                /* x48.c:417 - release raises no interrupt */
    }

    if (row < 0 || row >= X48_KB_ROWS)
        return;
    saturn.keybuf.rows[row] &= (short)~mask;
}

void x48_key_release_all(void)
{
    if (!s_ready)
        return;
    memset(saturn.keybuf.rows, 0, sizeof saturn.keybuf.rows);
}

/* --- state -------------------------------------------------------------- */

void x48_reset(bool cold)
{
    if (!s_ready)
        return;
    /* TODO(agape48): cold reset should wipe RAM the way ON+A+F does. do_reset()
     * is the warm path only; the cold path in the vendored tree runs through
     * the keyboard, so route it there rather than inventing a second one. */
    (void)cold;
    do_reset();
    s_dirty = true;
}

bool x48_save_state(void)
{
    if (!s_ready) {
        set_error("x48_save_state: core not running");
        return false;
    }
    if (!write_files()) {
        set_error("write_files failed - state directory not writable?");
        return false;
    }
    return true;
}

bool x48_reload_state(void)
{
    /* TODO(agape48): needed for the sync-conflict path in the README. It has to
     * tear the core down and bring it back up against the same paths, because
     * the vendored init_emulator() is the only thing that reads state files. */
    set_error("x48_reload_state: not implemented");
    return false;
}

uint64_t x48_state_fingerprint(void)
{
    struct stat st;
    char path[512];

    /* conf_filename is a bare leaf - "hp48" - because init.c pastes files_path
     * in front of it everywhere else. This did not, so it has been stat'ing a
     * file in the working directory since it was written, which is nowhere, and
     * hasExternalChange() has therefore always said no. It matters now that
     * each calculator has its own folder. */
    snprintf(path, sizeof path, "%s%s", files_path, conf_filename);
    if (stat(path, &st) != 0)
        return 0;
    return ((uint64_t)st.st_size << 32) ^ (uint64_t)st.st_mtime;
}

/* --- clipboard ---------------------------------------------------------- */

/* Item 3 settled these as: reals and strings move directly, everything else is
 * formatted and parsed by the ROM via DUP ->STR and STR->. Neither needs the
 * per-type decoders, so neither is written here yet. What they do need first is
 * the HP 48 to Unicode table, which does not exist in the tree. */

/* --- object interchange -------------------------------------------------- *
 *
 * The HP 48 binary transfer format, which is the only thing Agape48, Emu48,
 * Droid48, x48 and a real HP 48 over Kermit all already agree on: eight ASCII
 * bytes, "HPHP48-" and a ROM revision letter, then the object exactly as it
 * lives in memory - raw nibbles, low nibble of each byte first.
 *
 * A library is not a different format. It is an object whose type happens to be
 * library, so it travels in this same file; what differs is where it goes
 * afterwards, which is a port and a warm start, not a file question.
 *
 * The RPL primitives all come from the vendored binio.c and are not declared in
 * any header there, so they are declared here. Nothing in that file is edited:
 * read_bin_file() is deliberately NOT used, because it treats a file without
 * the header as a string, wraps the raw bytes in one, pushes it and reports
 * success - drop a JPEG on the calculator and it becomes a large string with no
 * complaint. Gert recorded that trap on 2026aug26. This rejects instead.
 * ------------------------------------------------------------------------ */

/* binio.c declares these three at file scope and puts them in no header, so
 * they are repeated here rather than reached for. Same definitions. */
typedef word_20       DWORD;
typedef unsigned char BYTE;
typedef unsigned int  UINT;

extern void  Npeek(BYTE *a, DWORD d, UINT s);
extern void  Nwrite(BYTE *a, DWORD d, UINT s);
extern DWORD Read5(DWORD d);
extern DWORD RPL_CreateTemp(DWORD l);
extern void  RPL_Push(DWORD n);

#define A48_HDR_LEN   8            /* "HPHP48" + '-' + revision letter */
#define A48_ADDR_END  0x100000     /* the Saturn's 20-bit address space */

/* The largest object we will move. A 48GX has 128 KB of RAM and up to 4 MB of
 * card, so this is generous for anything that can sit on the stack, and it
 * bounds every allocation below. */
#define A48_MAX_NIBS  (4u * 1024u * 1024u)

/* Nibble count of the object at o, or 0 if it does not fit inside avail.
 *
 * The vendored RPL_ObjectSize() takes a pointer and nothing else and recurses
 * through composites, so a truncated or hostile file walks off the end of the
 * buffer. This is the same walk with a length and a depth limit, which is what
 * lets Agape48 open a file somebody downloaded. Prologue values from rpl.h. */
static DWORD ob_size(const BYTE *o, DWORD avail, int depth)
{
    DWORD n, l = 0, i;

    if (depth > 64 || avail < 5)
        return 0;

    n = 0;
    for (i = 5; i-- > 0; )
        n = (n << 4) | o[i];

    switch (n) {
    case DOBINT:  l = 10; break;
    case DOREAL:  l = 21; break;
    case DOEREAL: l = 26; break;
    case DOCMP:   l = 37; break;
    case DOECMP:  l = 47; break;
    case DOCHAR:  l =  7; break;
    case DOEXT1:  l = 15; break;
    case DOROMP:  l = 11; break;
    case SEMI:    return 0;        /* end marker: zero length, on purpose */

    case DOLIST: case DOSYMB: case DOEXT: case DOCOL: {
        DWORD step = 5;
        l = 0;
        do {
            l += step;
            if (l > avail)
                return 0;
            step = ob_size(o + l, avail - l, depth + 1);
        } while (step);
        l += 5;                    /* the SEMI that ended the loop */
        break;
    }

    case DOIDNT: case DOLAM: case DOTAG: {
        DWORD body;
        if (avail < 7)
            return 0;
        n = 7 + ((DWORD)o[5] | ((DWORD)o[6] << 4)) * 2;
        if (n > avail)
            return 0;
        body = ob_size(o + n, avail - n, depth + 1);
        if (body == 0)
            return 0;
        l = n + body;
        break;
    }

    case DORRP: {                  /* directory */
        DWORD body;
        if (avail < 13)
            return 0;
        n = 0;
        for (i = 5; i-- > 0; )
            n = (n << 4) | o[8 + i];
        if (n == 0) { l = 13; break; }
        l = 8 + n;
        if (l + 2 > avail)
            return 0;
        n = ((DWORD)o[l] | ((DWORD)o[l + 1] << 4)) * 2 + 4;
        l += n;
        if (l > avail)
            return 0;
        body = ob_size(o + l, avail - l, depth + 1);
        if (body == 0)
            return 0;
        l += body;
        break;
    }

    case DOARRY: case DOLNKARRY: case DOCSTR: case DOHSTR: case DOGROB:
    case DOLIB:  case DOBAK:     case DOEXT0: case DOEXT2: case DOEXT3:
    case DOEXT4: case DOCODE:
        if (avail < 10)
            return 0;
        n = 0;
        for (i = 5; i-- > 0; )
            n = (n << 4) | o[5 + i];
        l = 5 + n;
        break;

    default:
        l = 5;                     /* an unknown prologue is a bare pointer */
        break;
    }

    return (l == 0 || l > avail) ? 0 : l;
}

/* The revision letter for the transfer header, read out of the ROM itself.
 *
 * The letter names the ROM revision of the machine that produced the file, and
 * ours is whichever ROM the user loaded. Hardcoding 'A' made every export
 * differ from its source at byte 8 and nowhere else - dogfood #15 line 11,
 * where Gert's own S6.LIB came in as HPHP48-M and went out as HPHP48-A.
 *
 * The ROM says so itself: "Version HP48-" appears in it as ordinary text, two
 * nibbles per character, and the character after it is the revision. In the
 * 48GX ROM this project runs on it is R, twice over. */
static char s_rom_rev = 'A';

static void detect_rom_revision(void)
{
    static const char key[] = "Version HP48-";
    const size_t klen = sizeof key - 1;
    size_t i, k;

    s_rom_rev = 'A';
    if (!saturn.rom || rom_size < (klen + 1) * 2)
        return;

    for (i = 0; i + (klen + 1) * 2 <= rom_size; i++) {
        for (k = 0; k < klen; k++) {
            unsigned c = (unsigned)saturn.rom[i + k * 2]
                       | ((unsigned)saturn.rom[i + k * 2 + 1] << 4);
            if (c != (unsigned char)key[k])
                break;
        }
        if (k == klen) {
            unsigned c = (unsigned)saturn.rom[i + klen * 2]
                       | ((unsigned)saturn.rom[i + klen * 2 + 1] << 4);
            if (c >= 'A' && c <= 'Z') {
                s_rom_rev = (char)c;
                return;
            }
        }
    }
}

/* True if stack level 1 holds anything at all. Asked before the file dialog
 * opens, so an empty stack is refused before the user has picked a name -
 * dogfood #15 line 6.
 *
 * The empty stack this has to catch is the real one: the ROM terminates the
 * stack with a null pointer, which is the same layout RPL_Push() walks, so
 * Read5() of it gives 0. It assumes a booted machine and says "yes" to the
 * uninitialised RAM of a calculator that has not started yet - measured, and
 * unreachable from a menu, since the menu needs a running calculator. Export
 * checks properly on its own account either way. */
bool x48_stack_has_object(void)
{
    DWORD stkp, addr;
    if (!s_ready)
        return false;
    stkp = Read5(DSKTOP);
    addr = Read5(stkp);
    return addr != 0 && addr < A48_ADDR_END;
}

bool x48_import_file(const char *path)
{
    FILE  *fp;
    long   len;
    BYTE  *raw = NULL, *nibs = NULL;
    DWORD  nib_count, size, addr;
    size_t got;
    long   i;

    if (!s_ready) { set_error("no calculator running"); return false; }

    if ((fp = fopen(path, "rb")) == NULL) {
        set_error("cannot open that file");
        return false;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) < 0
            || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        set_error("cannot read that file");
        return false;
    }
    if (len <= A48_HDR_LEN) {
        fclose(fp);
        set_error("that file is too short to be an HP 48 object");
        return false;
    }
    if ((DWORD)(len - A48_HDR_LEN) * 2u > A48_MAX_NIBS) {
        fclose(fp);
        set_error("that file is too large for the calculator");
        return false;
    }

    raw = (BYTE *)malloc((size_t)len);
    if (!raw) { fclose(fp); set_error("out of memory"); return false; }
    got = fread(raw, 1, (size_t)len, fp);
    fclose(fp);
    if (got != (size_t)len) { free(raw); set_error("cannot read that file"); return false; }

    if (memcmp(raw, "HPHP48-", 7) != 0) {
        free(raw);
        set_error("that is not an HP 48 object file - it has no \"HPHP48-\" header");
        return false;
    }

    /* Body to nibbles, low nibble of each byte first. */
    nib_count = (DWORD)(len - A48_HDR_LEN) * 2u;
    nibs = (BYTE *)malloc(nib_count);
    if (!nibs) { free(raw); set_error("out of memory"); return false; }
    for (i = 0; i < len - A48_HDR_LEN; i++) {
        BYTE b = raw[A48_HDR_LEN + i];
        nibs[i * 2]     = (BYTE)(b & 0x0f);
        nibs[i * 2 + 1] = (BYTE)(b >> 4);
    }
    free(raw);

    size = ob_size(nibs, nib_count, 0);
    if (size == 0) {
        free(nibs);
        set_error("that object is damaged or truncated");
        return false;
    }

    /* A transfer file holds exactly one object, so its size has to account for
     * very nearly the whole file - at most one byte of padding, which is what
     * an object with an odd nibble count leaves behind.
     *
     * Without this, random bytes behind a valid header are accepted: an
     * unrecognised prologue falls to the "bare pointer, five nibbles" case,
     * which is correct INSIDE a program - a program body is full of pointers to
     * ROM commands - and nonsense as a whole file. Measured: 40 bytes of
     * /dev/urandom with an HPHP48-A header imported happily before this, and
     * pushing a malformed object is how a real 48 gets a Memory Clear. */
    if (size + 2 < nib_count) {
        free(nibs);
        set_error("that file does not hold a single HP 48 object");
        return false;
    }

    addr = RPL_CreateTemp(size);
    if (addr == 0) {
        free(nibs);
        set_error("not enough calculator memory for that object");
        return false;
    }
    Nwrite(nibs, addr, size);
    free(nibs);
    RPL_Push(addr);
    s_dirty = true;                /* the stack display has to be redrawn */
    return true;
}

bool x48_export_file(const char *path)
{
    FILE  *fp;
    DWORD  stkp, addr, avail, size, i;
    BYTE  *nibs;
    bool   ok;

    if (!s_ready) { set_error("no calculator running"); return false; }

    /* DSKTOP holds the address of the stack; at that address is the 5-nibble
     * pointer to whatever is on level 1. RPL_Push() documents this layout from
     * the other side. A null pointer there means the stack is empty. */
    stkp = Read5(DSKTOP);
    addr = Read5(stkp);
    if (addr == 0 || addr >= A48_ADDR_END) {
        set_error("there is nothing on level 1 to export");
        return false;
    }

    avail = A48_ADDR_END - addr;
    if (avail > A48_MAX_NIBS)
        avail = A48_MAX_NIBS;

    nibs = (BYTE *)malloc(avail);
    if (!nibs) { set_error("out of memory"); return false; }
    Npeek(nibs, addr, avail);

    size = ob_size(nibs, avail, 0);
    if (size == 0) {
        free(nibs);
        set_error("level 1 does not hold an object this version can export");
        return false;
    }

    if ((fp = fopen(path, "wb")) == NULL) {
        free(nibs);
        set_error("cannot write that file");
        return false;
    }

    /* The revision letter comes from the loaded ROM - see detect_rom_revision. */
    {
        char hdr[A48_HDR_LEN + 1];
        snprintf(hdr, sizeof hdr, "HPHP48-%c", s_rom_rev);
        ok = fwrite(hdr, 1, A48_HDR_LEN, fp) == A48_HDR_LEN;
    }

    /* Nibbles back to bytes, low nibble first. An object with an odd nibble
     * count pads with a zero, which is what a real transfer does. */
    for (i = 0; ok && i < size; i += 2) {
        int b = nibs[i] | ((i + 1 < size ? nibs[i + 1] : 0) << 4);
        ok = fputc(b, fp) != EOF;
    }
    if (fclose(fp) != 0)
        ok = false;
    free(nibs);

    if (!ok) {
        remove(path);
        set_error("could not finish writing that file");
        return false;
    }
    return true;
}

size_t x48_stack_to_text(char *buf, size_t buflen)
{
    (void)buf; (void)buflen;
    set_error("clipboard: not implemented (needs the HP48-to-Unicode table)");
    return 0;
}

bool x48_text_to_stack(const char *utf8)
{
    (void)utf8;
    set_error("clipboard: not implemented (needs the Unicode-to-HP48 table)");
    return false;
}

/* --- beeper ------------------------------------------------------------- */

bool x48_take_beep(uint32_t *freq_hz, uint32_t *duration_ms)
{
    /* TODO(agape48): the vendored device.c raises the beep through
     * Java_org_ab_x48_X48_fillAudioData, which we do not compile. Trap it at
     * the OUT-register write instead and queue it here. */
    (void)freq_hz; (void)duration_ms;
    return false;
}

/* --- diagnostics -------------------------------------------------------- */

const char *x48_last_error(void)
{
    return s_error;
}

const char *x48_core_version(void)
{
    return "Droid48 fork of x48 (Dost 1994), vendored 2026aug28";
}
