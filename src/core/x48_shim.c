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
#include <stdlib.h>     /* malloc/free - implicitly declared before 2026sep02,
                         * which on a 64-bit build means a pointer truncated to
                         * int if the heap ever reaches above 4 GB. It never did
                         * here, but that is luck rather than design. */
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
/* La trigrafoj de la vendorita hp48char.h, kiel propra kopio sub alia nomo. La
 * originala tabelo loĝas en rpl.c, kaj unu referenco al ĝi tirus rpl.c en la
 * ligadon kun ĉiuj ĝiaj dependoj de la erarserĉilo, kiuj ne estas en la kerno. */
#define hp48_trans_tbl a48_trigraphs
#define DEFINE_TRANS_TABLE 1
#include "x48/hp48char.h"
#undef DEFINE_TRANS_TABLE
#undef hp48_trans_tbl

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
static unsigned long long s_instr_total;   /* see x48_instructions_total() */
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
    /* The LCD buffer is NOT part of the saved state - the ROM redraws it from
     * display memory whenever something changes, and a machine that was parked
     * in SHUTDN when it was saved changes nothing on the way back. So without
     * this the window keeps showing the LAST calculator it had: open somebody
     * else's from the shelf and their stack is on your screen, right up until
     * you press a key. update_display() paints the buffer from the machine we
     * have just loaded, which is what the ROM would have done itself. */
    update_display();
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
    /* Deliberately does NOT write the files, which it did until 2026sep03.
     *
     * Whether this calculator may be written is a question only the caller can
     * answer - it depends on whether we still hold the lock on its folder - and
     * saveState() is where that decision lives. A write down here happens
     * behind that decision's back, and it did: with saveState() correctly
     * refusing to touch a calculator handed to another machine, this line
     * wrote it anyway. Measured on a local shelf, the marker planted at sha
     * f42e0e7c... came back as this process's memory the moment the window
     * closed, with saveState() having already declined.
     *
     * Both callers save first and then shut down, so nothing is lost by this:
     * Agape48Engine::shutdownCore() and ~Agape48Engine() each call saveState()
     * on the line above their x48_shutdown(). One policy, one place. */
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
    s_instr_total += (unsigned long long)n;

    /* blockConditionVariable() clears exit_state when the machine parks in
     * SHUTDN. Restore it so the next slice can run, and report the state. */
    /* Woken by its own SHUTDN pass, with budget to spare: carry on rather than
     * making the machine wait for the next tick to act on the key. */
    if (!exit_state && x48_shutdn_woke && n < max_cycles) {
        exit_state = 1;
        const int more = agape48_emulate_slice(max_cycles - n);
        n += more;
        s_instr_total += (unsigned long long)more;
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

long x48_instructions_per_second(void)
{
    return s_ready ? saturn.i_per_s : 0;
}

/* OUR OWN COUNT, because the core's is not a total: schedule() resets
 * instructions to 1 every SCHED_INSTR_ROLLOVER. agape48_emulate_slice()
 * returns the exact number of step_instruction() calls it made, so adding
 * those up is both exact and immune to the rollover. Diagnostic: two saves
 * give instructions per wall second without trusting saturn.i_per_s, which
 * disagreed with the wall clock by a factor of six on 2026sep10. */
unsigned long long x48_instructions_total(void)
{
    return s_instr_total;
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

/* A content digest of the calculator's RAM, over exactly the nibbles
 * write_files() writes - write_mem_file(fnam, saturn.ram, opt_gx ? RAM_SIZE_GX
 * : RAM_SIZE_SX) - so an unchanged digest means the "ram" file would come out
 * byte for byte identical.
 *
 * This exists because x48_state_fingerprint() cannot answer the question. That
 * one stats the hp48 file and returns (size << 32) ^ mtime, so it describes the
 * DISK. What the caller needs before writing 131,072 bytes into a synced folder
 * is whether the MACHINE changed, and RAM is the only saved thing that does not
 * move while nothing is being entered - the CPU state in hp48 ticks over even
 * on an idle calculator.
 *
 * FNV-1a: about 130 kB of sequential reads, well under a millisecond, against a
 * write of the same size plus whatever the sync client then does with it.
 *
 * Returns 0 for "no opinion" - no calculator, or no RAM allocated. Since 0 is
 * also a legal digest value, a real digest of 0 is reported as 1 rather than
 * silently meaning "don't know".
 */
uint64_t x48_ram_digest(void)
{
    const unsigned char *p = (const unsigned char *)saturn.ram;
    long                 n = opt_gx ? RAM_SIZE_GX : RAM_SIZE_SX;
    uint64_t             h = 14695981039346656037ULL;   /* FNV-1a offset basis */

    if (!s_ready || p == NULL)
        return 0;
    while (n-- > 0) {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;                          /* FNV-1a prime */
    }
    return h != 0 ? h : 1;
}

bool x48_reload_state(void)
{
    const char *why;

    if (!s_ready) {
        set_error("x48_reload_state: no calculator running");
        return false;
    }

    /* Throw away what is in memory and read the files again. Nothing is
     * written: the whole point is that somebody else's version of this
     * calculator is now on disk and ours is the one being discarded.
     *
     * read_files() mallocs all four of these and assigns over the old pointers
     * without looking, so they have to go first or every reload leaks a ROM,
     * a RAM image and two card ports - about 650 KB a time. */
    free(saturn.rom);   saturn.rom   = NULL;
    free(saturn.ram);   saturn.ram   = NULL;
    free(saturn.port1); saturn.port1 = NULL;
    free(saturn.port2); saturn.port2 = NULL;

    why = agape48_init_emulator();
    if (why) {
        snprintf(s_error, sizeof s_error, "x48_reload_state: %s", why);
        s_ready = false;               /* there is no calculator now */
        return false;
    }
    init_active_stuff();
    update_display();               /* see the note in x48_init() */
    agape48_emulate_begin();
    detect_rom_revision();
    s_dirty = true;
    s_asleep = false;
    set_error("");
    return true;
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
 * complaint. That trap was recorded on 2026aug26. This rejects instead.
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
extern void  Write5(DWORD d, DWORD n);

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
 * where a real S6.LIB came in as HPHP48-M and went out as HPHP48-A.
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

/* Legas kaj kontrolas objektdosieron sen tuŝi la kalkulilon: la komuna parto de
 * x48_import_file() kaj x48_object_file_loadable(). Je sukceso *out estas la
 * objekto kiel duonbajtoj, kaj la vokanto liberigas ĝin. */
static bool read_object_file(const char *path, BYTE **out, DWORD *out_size)
{
    FILE  *fp;
    long   len;
    BYTE  *raw = NULL, *nibs = NULL;
    DWORD  nib_count, size;
    size_t got;
    long   i;

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
    *out = nibs;
    *out_size = size;
    return true;
}

/* Ĉu objekto de size duonbajtoj trovas lokon? La sama kalkulo kiel
 * RPL_CreateTemp() kaj RPL_Push(): la objekto kun sia ligkampo kaj marko inter
 * la reirstako kaj la datumstako, plus unu nova stakloko. */
static bool fits_in_memory(DWORD size)
{
    return Read5(RSKTOP) + size + 6 + 5 <= Read5(DSKTOP) && Read5(AVMEM) >= 1;
}

bool x48_object_file_loadable(const char *path)
{
    BYTE  *nibs = NULL;
    DWORD  size = 0;

    if (!read_object_file(path, &nibs, &size))
        return false;
    free(nibs);
    if (s_ready && !fits_in_memory(size)) {
        set_error("not enough calculator memory for that object");
        return false;
    }
    return true;
}

bool x48_import_file(const char *path)
{
    BYTE  *nibs = NULL;
    DWORD  size = 0, addr;

    if (!s_ready) { set_error("no calculator running"); return false; }
    if (!read_object_file(path, &nibs, &size))
        return false;

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

/* --- la tondujo ----------------------------------------------------------
 *
 * NIVELO 1, NE LA TUTA STAKO, malgraŭ la nomo de la menuero, laŭ la sama regulo
 * kiun la elporta komando ĉiam havis: reela nombro aŭ ĉeno, kiel teksto kiun
 * homo rekonas - "1701", "-2.5E-9", "« 1 + »". Ĉiu alia tipo estas malkompila
 * tasko, kaj la malkompililo de RPL estas la ROM mem: →STR faras ĉenon el kia
 * ajn objekto kaj STR→ faras la malon. Tial la aŭtomataj →STR kaj STR→ pli sube
 * enhavas nenian malkompililon; ili petas la ROM-on.
 *
 * La formato de nombro estas tiu de la maŝino: 5 duonbajtoj da prologo, 3
 * ciferoj de eksponento en dekkomplemento, 12 ciferoj de mantiso, ĉiuj kun la
 * malplej signifa duonbajto UNUE, poste 1 duonbajto de signo - 21 duonbajtoj,
 * kion ob_size() atendas de DOREAL. Ĉeno estas 5-duonbajta longo kiu kalkulas
 * sin mem, poste du duonbajtoj por ĉiu signo.
 *
 * LA EKSPONENTO VENAS UNUE, kaj tio indis mezuron prefere ol memoron: kun la
 * mantiso legita de la malĝusta fino, 1701 kopiiĝis kiel "1.00000000003E170",
 * kio estas la kialo, ke la menuero diras laŭte kion ĝi metis en la tondujon.
 *
 * LA SIGNARO. La HP 48 kongruas kun ASCII de 32 ĝis 126 kaj kun ISO 8859-1 de
 * 160 ĝis 255. La 32 signoj de 128 ĝis 159 estas propraj, kaj la vendorita
 * hp48char.h donas al ĉiu sian trigrafon, la askian formon de la transiga
 * formato; s_hp_high aldonas la Unikodan formon en la sama ordo. Linifino estas
 * signo 10. La ceteraj regsignoj eliras kiel siaj trigrafoj (\001), ĉar ili
 * havas nenian videblan Unikodan formon.
 *
 * Enire la trigrafoj validas laŭ la kaplinio "%%HP: T(n)A(a)F(f);" kiam ĝi
 * ĉeestas - T(0) kaj T(1) tradukas nur linifinojn, T(2) aldonas 128..159, T(3)
 * ĉiujn - kaj sen kaplinio kiel T(3), ĉar tiel aspektas la programoj en
 * hpcalc.org kaj en forumoj. Signo sen HP 48-ekvivalento rifuzas la tutan
 * tekston, kaj la eraro nomas ĝin: diveni estus pli malbone ol rifuzi. La
 * A() kaj F() de la kaplinio restas neuzataj; la angulan reĝimon kaj la
 * dekuman signon decidas la kalkulilo mem. */

/* 128..159 en UTF-8, en la ordo de hp48char.h. */
static const char *const s_hp_high[32] = {
    "\xE2\x88\xA1",    /* 128  ∡  \<) */
    "x\xCC\x84",       /* 129  x̄  \x- */
    "\xE2\x88\x87",    /* 130  ∇  \.V */
    "\xE2\x88\x9A",    /* 131  √  \v/ */
    "\xE2\x88\xAB",    /* 132  ∫  \.S */
    "\xCE\xA3",        /* 133  Σ  \GS */
    "\xE2\x96\xB6",    /* 134  ▶  \|> */
    "\xCF\x80",        /* 135  π  \pi */
    "\xE2\x88\x82",    /* 136  ∂  \.d */
    "\xE2\x89\xA4",    /* 137  ≤  \<= */
    "\xE2\x89\xA5",    /* 138  ≥  \>= */
    "\xE2\x89\xA0",    /* 139  ≠  \=/ */
    "\xCE\xB1",        /* 140  α  \Ga */
    "\xE2\x86\x92",    /* 141  →  \-> */
    "\xE2\x86\x90",    /* 142  ←  \<- */
    "\xE2\x86\x93",    /* 143  ↓  \|v */
    "\xE2\x86\x91",    /* 144  ↑  \|^ */
    "\xCE\xB3",        /* 145  γ  \Gg */
    "\xCE\xB4",        /* 146  δ  \Gd */
    "\xCE\xB5",        /* 147  ε  \Ge */
    "\xCE\xB7",        /* 148  η  \Gn */
    "\xCE\xB8",        /* 149  θ  \Gh */
    "\xCE\xBB",        /* 150  λ  \Gl */
    "\xCF\x81",        /* 151  ρ  \Gr */
    "\xCF\x83",        /* 152  σ  \Gs */
    "\xCF\x84",        /* 153  τ  \Gt */
    "\xCF\x89",        /* 154  ω  \Gw */
    "\xCE\x94",        /* 155  Δ  \GD */
    "\xCE\xA0",        /* 156  Π  \PI */
    "\xCE\xA9",        /* 157  Ω  \GW */
    "\xE2\x96\xA0",    /* 158  ■  \[] */
    "\xE2\x88\x9E",    /* 159  ∞  \oo */
};

/* Unu HP 48-signo al UTF-8 en out, kiu havas almenaŭ 8 bajtojn. */
static size_t hp_to_utf8(unsigned c, char *out)
{
    const char *t;
    size_t n;

    if (c == 10) { out[0] = '\n'; return 1; }
    if (c >= 32 && c <= 126) { out[0] = (char)c; return 1; }
    if (c >= 128 && c <= 159) {
        t = s_hp_high[c - 128];
        n = strlen(t);
        memcpy(out, t, n);
        return n;
    }
    if (c >= 160 && c <= 255) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    t = a48_trigraphs[c & 0xFF].trans;          /* regsignoj kaj 127 */
    n = t ? strlen(t) : 0;
    if (n == 0 || n > 7) { out[0] = '?'; return 1; }
    memcpy(out, t, n);
    return n;
}

/* Unu kodpunkto el UTF-8; redonas la konsumitajn bajtojn, aŭ 0 se nevalida. */
static size_t utf8_decode(const unsigned char *s, size_t len, unsigned long *cp)
{
    if (len >= 1 && s[0] < 0x80) { *cp = s[0]; return 1; }
    if (len >= 2 && (s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return *cp >= 0x80 ? 2 : 0;
    }
    if (len >= 3 && (s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80
                 && (s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x0F) << 12)
            | ((unsigned long)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return *cp >= 0x800 ? 3 : 0;
    }
    if (len >= 4 && (s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80
                 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned long)(s[0] & 0x07) << 18)
            | ((unsigned long)(s[1] & 0x3F) << 12)
            | ((unsigned long)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return (*cp >= 0x10000 && *cp <= 0x10FFFF) ? 4 : 0;
    }
    return 0;
}

/* HP 48-kodo de unu el la 32 propraj signoj, aŭ -1. x̄ havas du kodpunktojn
 * kaj estas traktata de la vokanto. */
static int hp_high_from_cp(unsigned long cp)
{
    unsigned long c;
    size_t n;
    int i;

    for (i = 0; i < 32; i++) {
        n = strlen(s_hp_high[i]);
        if (utf8_decode((const unsigned char *)s_hp_high[i], n, &c) == n && c == cp)
            return 128 + i;
    }
    if (cp == 0x2220) return 128;               /* ∠, la ofta formo de ∡ */
    if (cp == 0x03BC) return 181;               /* greka μ, la sama signo kiel µ */
    return -1;
}

/* La plej longa trigrafo de hp48_trans_tbl ĉe s, laŭ la tradukreĝimo. */
static int hp_from_trigraph(const char *s, size_t len, int mode, size_t *used)
{
    const char *t;
    size_t n, bestn = 0;
    int c, best = -1;

    for (c = 0; c < 256; c++) {
        t = a48_trigraphs[c].trans;
        if (!t || (mode == 2 && c >= 160))
            continue;
        n = strlen(t);
        if (n <= len && n > bestn && memcmp(s, t, n) == 0) {
            best = c;
            bestn = n;
        }
    }
    *used = bestn;
    return best;
}

/* Kie la korpo komenciĝas post eventuala kaplinio "%%HP: T(3)A(D)F(.);", kaj
 * kun kiu tradukreĝimo. Sen kaplinio: la tuta teksto, reĝimo 3. */
static const char *hp_header(const char *text, int *mode)
{
    const char *p = text, *end, *nl, *t;

    *mode = 3;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB
        && (unsigned char)p[2] == 0xBF)
        p += 3;                                  /* BOM */
    if (strncmp(p, "%%HP:", 5) != 0)
        return p;
    end = strchr(p, ';');
    nl  = strchr(p, '\n');
    if (!end || (nl && nl < end))
        return p;                                /* komenciĝas tiel, sed ne estas kaplinio */
    for (t = p + 5; t + 3 < end; t++)
        if (t[0] == 'T' && t[1] == '(' && t[2] >= '0' && t[2] <= '3' && t[3] == ')') {
            *mode = t[2] - '0';
            break;
        }
    p = end + 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

/* Teksto al HP 48-bajtoj en out, kiu havas almenaŭ strlen(body) bajtojn.
 * Redonas la nombron da bajtoj, aŭ -1 kun eraro kiu nomas la signon. */
static long utf8_to_hp(const char *body, int mode, BYTE *out)
{
    const unsigned char *s = (const unsigned char *)body;
    size_t len = strlen(body), i = 0, n;

    /* La lasta linifino de tekstdosiero estas konvencio de la dosiero, ne enhavo:
     * sen ĉi tio ĝi aperis kiel ■ ĉe la fino de ĉiu demetita programo. */
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        len--;
    unsigned long cp;
    char msg[160], shown[8];
    long o = 0;
    int c;

    while (i < len) {
        unsigned char b = s[i];
        if (b == '\r') { out[o++] = 10; i += (i + 1 < len && s[i + 1] == '\n') ? 2 : 1; continue; }
        if (b == '\n') { out[o++] = 10; i++; continue; }
        if (b == '\t') { out[o++] = 32; i++; continue; }
        if (b == '\\' && mode >= 2) {
            c = hp_from_trigraph(body + i, len - i, mode, &n);
            if (c >= 0) { out[o++] = (BYTE)c; i += n; continue; }
            out[o++] = b; i++;
            continue;
        }
        if (b == 'x' && i + 2 < len && s[i + 1] == 0xCC && s[i + 2] == 0x84) {
            out[o++] = 129; i += 3;              /* x kaj kombina makrono: x̄ */
            continue;
        }
        if (b >= 32 && b <= 126) { out[o++] = b; i++; continue; }
        if (b < 32 || b == 127) {
            snprintf(msg, sizeof msg, "the text holds a control character "
                     "(code %u) that the calculator cannot hold", (unsigned)b);
            set_error(msg);
            return -1;
        }
        n = utf8_decode(s + i, len - i, &cp);
        if (n == 0) { set_error("the text is not valid UTF-8"); return -1; }
        c = (cp >= 0xA0 && cp <= 0xFF) ? (int)cp : hp_high_from_cp(cp);
        if (c < 0) {
            memset(shown, 0, sizeof shown);
            memcpy(shown, body + i, n);
            snprintf(msg, sizeof msg, "\"%s\" (U+%04lX) has no HP 48 "
                     "character, so the text was not loaded", shown, cp);
            set_error(msg);
            return -1;
        }
        out[o++] = (BYTE)c;
        i += n;
    }
    return o;
}

#define A48_REAL_NIBS 21
#define A48_MANT_DIGITS 12

/* Digits of a real, most significant first, plus its exponent and sign. */
static void real_digits(const BYTE *o, char *digits, int *exp10, int *neg)
{
    int i, e;
    for (i = 0; i < A48_MANT_DIGITS; i++)
        digits[i] = (char)('0' + (o[19 - i] & 0x0f));   /* o[19] is the leading digit */
    digits[A48_MANT_DIGITS] = '\0';
    e = (o[5] & 0x0f) + (o[6] & 0x0f) * 10 + (o[7] & 0x0f) * 100;
    if (e >= 500)
        e -= 1000;                 /* ten's complement, the machine's own form */
    *exp10 = e;
    *neg = (o[20] & 0x0f) != 0;
}

static size_t real_to_text(const BYTE *o, char *out, size_t outlen)
{
    char digits[A48_MANT_DIGITS + 1], tmp[64];
    int exp10, neg, sig, i, at = 0;

    real_digits(o, digits, &exp10, &neg);
    sig = A48_MANT_DIGITS;
    while (sig > 1 && digits[sig - 1] == '0')
        sig--;                     /* trailing zeros carry no information */

    if (neg)
        tmp[at++] = '-';
    if (exp10 >= 0 && exp10 <= 11) {
        for (i = 0; i <= exp10; i++)
            tmp[at++] = (i < sig) ? digits[i] : '0';
        if (sig > exp10 + 1) {
            tmp[at++] = '.';
            for (i = exp10 + 1; i < sig; i++)
                tmp[at++] = digits[i];
        }
    } else if (exp10 < 0 && exp10 >= -11) {
        tmp[at++] = '0';
        tmp[at++] = '.';
        for (i = 0; i < -exp10 - 1; i++)
            tmp[at++] = '0';
        for (i = 0; i < sig; i++)
            tmp[at++] = digits[i];
    } else {
        tmp[at++] = digits[0];
        if (sig > 1) {
            tmp[at++] = '.';
            for (i = 1; i < sig; i++)
                tmp[at++] = digits[i];
        }
        at += (int)snprintf(tmp + at, sizeof(tmp) - (size_t)at, "E%d", exp10);
    }
    tmp[at] = '\0';

    if (out && (size_t)at + 1 <= outlen)
        memcpy(out, tmp, (size_t)at + 1);
    return (size_t)at + 1;         /* including the terminator */
}

size_t x48_stack_to_text(char *buf, size_t buflen)
{
    DWORD stkp, addr, avail, size, prologue, i, chars;
    BYTE *nibs;
    size_t need = 0;

    if (!s_ready) { set_error("no calculator running"); return 0; }

    stkp = Read5(DSKTOP);
    addr = Read5(stkp);
    if (addr == 0 || addr >= A48_ADDR_END) {
        set_error("there is nothing on level 1 to copy");
        return 0;
    }
    avail = A48_ADDR_END - addr;
    if (avail > A48_MAX_NIBS)
        avail = A48_MAX_NIBS;
    nibs = (BYTE *)malloc(avail);
    if (!nibs) { set_error("out of memory"); return 0; }
    Npeek(nibs, addr, avail);

    size = ob_size(nibs, avail, 0);
    if (size == 0) {
        free(nibs);
        set_error("level 1 does not hold an object this version understands");
        return 0;
    }

    prologue = 0;
    for (i = 5; i-- > 0; )
        prologue = (prologue << 4) | nibs[i];

    if (prologue == DOREAL && size >= A48_REAL_NIBS) {
        need = real_to_text(nibs, buf, buflen);
    } else if (prologue == DOCSTR && size >= 10) {
        DWORD len5 = 0;
        size_t at = 0, k;
        char one[8];
        for (i = 5; i-- > 0; )
            len5 = (len5 << 4) | nibs[5 + i];
        chars = (len5 >= 5) ? (len5 - 5) / 2 : 0;
        if (10 + chars * 2 > size)
            chars = (size - 10) / 2;
        for (i = 0; i < chars; i++) {
            unsigned c = (unsigned)nibs[10 + i * 2]
                       | ((unsigned)nibs[10 + i * 2 + 1] << 4);
            k = hp_to_utf8(c, one);
            if (buf && at + k < buflen)
                memcpy(buf + at, one, k);
            at += k;
        }
        need = at + 1;
        if (buf && need <= buflen)
            buf[at] = '\0';
    } else {
        free(nibs);
        set_error("Copy handles a number or a text string on level 1. "
                  "Automatic \xE2\x86\x92" "STR on Copy, in Settings, copies "
                  "every other kind of object as text, and Export from stack "
                  "to file writes it whole.");
        return 0;
    }

    free(nibs);
    return need;
}

/* Text to a real, or 0 nibbles if it is not a number this can read.
 *
 * Every digit is collected in order, with a note of how many came before the
 * point; the exponent then falls out of where the first digit that is not a
 * zero sits relative to it. That is one rule for "1701", ".5", "0.007" and
 * "12.5E2" together, and it is why there is no special case here for any of
 * them. */
static int parse_real(const char *t, BYTE *out)
{
    char all[64];
    int neg = 0, count = 0, intCount = -1, seen = 0;
    int expGiven = 0, expNeg = 0, first, sig, exp10, i;
    const char *p = t;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }

    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            if (count < (int)sizeof(all) - 1)
                all[count] = *p;
            count++;
            seen = 1;
        } else if (*p == '.' && intCount < 0) {
            intCount = count;
        } else if ((*p == 'e' || *p == 'E') && seen) {
            p++;
            if (*p == '+' || *p == '-') { expNeg = (*p == '-'); p++; }
            if (*p < '0' || *p > '9')
                return 0;
            for (; *p >= '0' && *p <= '9'; p++) {
                expGiven = expGiven * 10 + (*p - '0');
                if (expGiven > 9999)
                    return 0;
            }
            break;
        } else {
            return 0;              /* not a plain number */
        }
    }
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p != '\0' || !seen || count > (int)sizeof(all) - 1)
        return 0;
    if (intCount < 0)
        intCount = count;          /* no point at all: it is all integer */
    all[count] = '\0';

    for (first = 0; first < count && all[first] == '0'; first++)
        ;
    if (first == count) {          /* every digit was a zero */
        first = 0;
        sig = 1;
        all[0] = '0';
        exp10 = 0;
        neg = 0;                   /* there is no negative zero on a 48 */
    } else {
        sig = count - first;
        exp10 = intCount - 1 - first + (expNeg ? -expGiven : expGiven);
    }
    if (exp10 > 499 || exp10 < -499)
        return 0;
    if (sig > A48_MANT_DIGITS)
        sig = A48_MANT_DIGITS;

    for (i = 0; i < 5; i++)
        out[i] = (BYTE)((DOREAL >> (i * 4)) & 0x0f);
    for (i = 0; i < A48_MANT_DIGITS; i++) {
        int d = (i < sig) ? (all[first + i] - '0') : 0;
        out[19 - i] = (BYTE)d;     /* most significant at 19, as read back */
    }
    {
        int e = exp10 < 0 ? exp10 + 1000 : exp10;
        out[5] = (BYTE)(e % 10);
        out[6] = (BYTE)((e / 10) % 10);
        out[7] = (BYTE)((e / 100) % 10);
    }
    out[20] = (BYTE)(neg ? 9 : 0);
    return A48_REAL_NIBS;
}

bool x48_text_loadable(const char *utf8)
{
    BYTE real[A48_REAL_NIBS];
    const char *body;
    BYTE *tmp;
    long n;
    int mode;

    if (!utf8 || !*utf8) { set_error("there is no text"); return false; }
    body = hp_header(utf8, &mode);
    if (parse_real(body, real) > 0)
        return !s_ready || fits_in_memory(A48_REAL_NIBS);
    if (strlen(body) > (A48_MAX_NIBS / 2u) - 16u) {
        set_error("that is too much text for the calculator");
        return false;
    }
    tmp = (BYTE *)malloc(strlen(body) + 1);
    if (!tmp) { set_error("out of memory"); return false; }
    n = utf8_to_hp(body, mode, tmp);
    free(tmp);
    if (n < 0)
        return false;
    if (s_ready && !fits_in_memory(10 + (DWORD)n * 2)) {
        set_error("not enough calculator memory for that text");
        return false;
    }
    return true;
}

bool x48_text_to_stack(const char *utf8)
{
    BYTE  real[A48_REAL_NIBS];
    BYTE *nibs, *chars;
    DWORD size, addr, i;
    const char *body;
    long  len;
    int   n, mode;

    if (!s_ready) { set_error("no calculator running"); return false; }
    if (!utf8 || !*utf8) { set_error("there is nothing on the clipboard"); return false; }

    body = hp_header(utf8, &mode);
    n = parse_real(body, real);
    if (n > 0) {
        nibs = (BYTE *)malloc((size_t)n);
        if (!nibs) { set_error("out of memory"); return false; }
        memcpy(nibs, real, (size_t)n);
        size = (DWORD)n;
    } else {
        /* Ne nombro, do ĉeno - kion kalkulilo povas honeste fari el ajna
         * teksto, kaj kion vera 48 faras kiam teksto alvenas per la kablo. */
        if (strlen(body) > (A48_MAX_NIBS / 2u) - 16u) {
            set_error("that is too much text for the calculator");
            return false;
        }
        chars = (BYTE *)malloc(strlen(body) + 1);
        if (!chars) { set_error("out of memory"); return false; }
        len = utf8_to_hp(body, mode, chars);
        if (len < 0) { free(chars); return false; }
        size = 10 + (DWORD)len * 2;
        nibs = (BYTE *)malloc(size);
        if (!nibs) { free(chars); set_error("out of memory"); return false; }
        for (i = 0; i < 5; i++)
            nibs[i] = (BYTE)((DOCSTR >> (i * 4)) & 0x0f);
        {
            DWORD field = 5 + (DWORD)len * 2;   /* la longo kalkulas sin mem */
            for (i = 0; i < 5; i++)
                nibs[5 + i] = (BYTE)((field >> (i * 4)) & 0x0f);
        }
        for (i = 0; i < (DWORD)len; i++) {
            nibs[10 + i * 2]     = (BYTE)(chars[i] & 0x0f);
            nibs[10 + i * 2 + 1] = (BYTE)(chars[i] >> 4);
        }
        free(chars);
    }

    addr = RPL_CreateTemp(size);
    if (addr == 0) {
        free(nibs);
        set_error("not enough calculator memory for that");
        return false;
    }
    Nwrite(nibs, addr, size);
    free(nibs);
    RPL_Push(addr);
    s_dirty = true;
    return true;
}

uint32_t x48_level1_address(void)
{
    return x48_stack_has_object() ? (uint32_t)Read5(Read5(DSKTOP)) : 0;
}

bool x48_level1_is_string(void)
{
    if (!x48_stack_has_object())
        return false;
    return Read5(Read5(Read5(DSKTOP))) == DOCSTR;
}

/* --- preteco por ricevi objekton ----------------------------------------
 *
 * SES DUONBAJTOJ EL LA SISTEMA RAM, kaj la dormo de la Saturn. Mezurite en
 * 2026sep13 per senkapa kalkulilo, kiu estis kondukita tra dek tri statoj - la
 * stako, komandlinio, programa kaj alfa enigo, la redaktilo, MODES, MEMORY,
 * CHARS, la interaga stako, EQUATION, PICTURE, HALT, kuranta programo, OFF - kun
 * RAM-kopio en ĉiu, kaj la ekrano bildigita por kontroli, ke ĉiu stato estis tiu
 * kiun ĝia nomo diras. Poste la sama sur la GX-revizioj K, L, M, P kaj R, kiuj
 * donis la samajn valorojn en ĉiu stato.
 *
 *            stako  komandlinio  formularo/aplikaĵo  mesaĝo sur stako  eraro supre
 *   80801      4         4              0                  0              4
 *   80805      1         3              1                  1              1
 *   80806      2         2              0                  2              2
 *   8080A      4         5            e / f                4              4
 *   8080B      0         0            0 / 1 / 8            0              0
 *   8080C      1         1              0                  1              0
 *
 * "Mesaĝo sur stako" estas ekzemple "Eq: Ptype: FUNCTION" post PLOT; "eraro
 * supre" estas "+ Error: Too Few Arguments" en la statusaj linioj. Ambaŭ
 * malaperas je la sekva klavo. Post malsukcesa STR→ la teksto estas denove sur
 * nivelo 1 kiel ĉeno, kaj la eraro staras supre.
 *
 * 8080C NE ESTAS LEGATA. Kalkulilo, kies stako montras ses nivelojn sen statusaj
 * linioj kaj algebraĵojn kiel frakciojn - laboro de biblioteko, ĉar fabrika 48GX
 * ne faras tion - tenas 8080C je 0 dum ĝi atendas ĉe la stako. Mezurite
 * 2026sep13 en la konservita RAM de tri tiaj kalkuliloj, kaj sur kopio de unu el
 * ili tra la stako, eraro supre, komandlinio kaj ON: 0 en ĉiu, krom en la
 * redaktilo. Tie 8080C distingas nenion, kaj sur fabrika kalkulilo ĝi distingas
 * nur la eraron supre, kiu ne malhelpas la stakon ricevi objekton - la ON-premo
 * post la ŝarĝo forigas la mesaĝon. 80801 restas 4 sur tiu kalkulilo, do
 * "mesaĝo sur stako" plu estas rifuzata.
 *
 * 80806 portas ankaŭ 1USR en sia plej malalta bito, kaj tio ne gravas ĉi tie.
 * HALT havas ĝuste la valorojn de la stako - nur la reirstako estas pli
 * profunda - kaj estas preta laŭ intenco: la tuta celo de HALT estas labori
 * per la stako dum la programo atendas. Malŝaltita kalkulilo ankaŭ aspektas
 * kiel la stako, tial display.on. La profundo de la reirstako (RSKTOP -
 * TEMPTOP: 40 duonbajtoj ĉe la stako) ne estas uzata; la ses duonbajtoj sufiĉas
 * por ĉiu mezurita stato, kaj numero kiu dependas de profundo estus pli
 * facile rompebla. */

#define A48_UI_A   0x80801
#define A48_UI_B   0x80805
#define A48_UI_C   0x80806
#define A48_UI_D   0x8080A
#define A48_UI_E   0x8080B
#define A48_USERF  0x80852         /* bito 1: USER, ĉu 1USR ĉu ŝlosita */

static unsigned ram_nib(DWORD a)
{
    BYTE b;
    Npeek(&b, a, 1);
    return (unsigned)(b & 0x0f);
}

x48_readiness_t x48_readiness(void)
{
    if (!s_ready)    return X48_NOT_RUNNING;
    if (!opt_gx)     return X48_NOT_GX;
    if (!s_asleep)   return X48_BUSY;
    if (!display.on) return X48_OFF;
    if (ram_nib(A48_UI_B) & 2)
        return X48_EDITING;
    if (!(ram_nib(A48_UI_C) & 2) || ram_nib(A48_UI_D) != 4 || ram_nib(A48_UI_E) != 0)
        return X48_ELSEWHERE;
    if (ram_nib(A48_UI_A) != 4)
        return X48_MESSAGE;
    return X48_READY;
}

bool x48_user_mode(void)
{
    return s_ready && opt_gx && (ram_nib(A48_USERF) & 2) != 0;
}

/* --- aŭtomataj →STR kaj STR→ ----------------------------------------------
 *
 * La montriloj estas tiuj, kiujn la kalkulilo mem kompilis, kiam oni tajpis
 * « DUP →STR » kaj « STR→ » kaj legis la programojn el la RAM: la samaj sur
 * K, L, M, P kaj R. Programo estas DOCOL, la montriloj, SEMI. */

static const DWORD s_tostr_body[] = { 0x2361E, 0x1FB87, 0x1CB0B, 0x23639 };
static const DWORD s_strto_body[] = { 0x2361E, 0x1CB26, 0x23639 };

static bool push_program(const DWORD *body, int count)
{
    BYTE  nibs[5 * 8];
    DWORD size = (DWORD)(count + 2) * 5, addr, v;
    int   i, k;

    if (x48_readiness() != X48_READY) {
        set_error("the calculator is not waiting at the stack");
        return false;
    }
    for (i = 0; i < count + 2; i++) {
        v = (i == 0) ? DOCOL : (i == count + 1) ? SEMI : body[i - 1];
        for (k = 0; k < 5; k++)
            nibs[i * 5 + k] = (BYTE)((v >> (k * 4)) & 0x0f);
    }
    addr = RPL_CreateTemp(size);
    if (addr == 0) {
        set_error("not enough calculator memory for that");
        return false;
    }
    Nwrite(nibs, addr, size);
    RPL_Push(addr);
    s_dirty = true;
    return true;
}

bool x48_push_tostr_program(void) { return push_program(s_tostr_body, 4); }
bool x48_push_strto_program(void) { return push_program(s_strto_body, 3); }

bool x48_drop_level1(void)
{
    DWORD stkp;

    if (!x48_stack_has_object()) {
        set_error("there is nothing on level 1");
        return false;
    }
    stkp = Read5(DSKTOP);
    Write5(DSKTOP, stkp + 5);
    Write5(AVMEM, Read5(AVMEM) + 1);
    s_dirty = true;
    return true;
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
