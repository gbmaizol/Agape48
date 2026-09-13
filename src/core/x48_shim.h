/* ---------------------------------------------------------------------------
 * x48_shim.h - la SOLA surfaco de la C-motoro x48 kiun la C++ de Agape48 vidas.
 *
 * Kial kudro entute: x48 (Dost), x48ng (Le Moine) kaj Droid48 ĉiu prezentas
 * alian aron da mallokaj variabloj kaj enirpunktoj, kaj ĉiuj tri fingrumas
 * komunan strukturon `saturn` el sia fasada kodo. Adapti ĉiun forkon malantaŭ ĉi tiuj
 * ~16 funkcioj signifas ke ŝanĝo de la fonto estas reskribo de x48_shim.c kaj
 * de nenio alia - neniam de la ponto, neniam de la QML.
 *
 * Fadena kontrakto: ĉiu funkcio ĉi tie devas esti vokata el nur unu fadeno.
 * Agape48 vokas ilin el la fasada fadeno de Qt (vidu Agape48Engine).
 * ------------------------------------------------------------------------- */
#ifndef AGAPE48_X48_SHIM_H
#define AGAPE48_X48_SHIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* La LCD de HP 48 estas 131x64 videbla. La ekranpelilo de Saturn povas
 * deŝovi la komencon de la skanlinio, do la kadrobufro estas trolarĝa kaj la
 * videbla fenestro estas raportata po kadro. */
#define X48_LCD_WIDTH       131
#define X48_LCD_HEIGHT       64
#define X48_LCD_STRIDE      144
#define X48_LCD_PIXELS      (X48_LCD_STRIDE * X48_LCD_HEIGHT)

/* Klavara matrico: 9 eliraj vicoj stiritaj de la Saturn, ses eniraj kolumnoj
 * relegataj kiel bitoj 0x01..0x20. Konfirmita kontraŭ la enkorpigita tabelo
 * buttons[] ĉe x48.c:233 - vicoj 1, 2 kaj 3 havas la sesan klavon (SHR, SHL,
 * ALPHA), la aliaj ses vicoj havas kvin.
 *
 * ON ne estas en la matrico. x48 donas al ĝi kodon 0x8000 kaj metas tiun biton
 * en ĈIUJ naŭ vicoj (x48.c:381), do la masko portas la signifon kaj la
 * vic-argumento estas ignorata: donu X48_KB_MASK_ON al x48_key_down/up per unu
 * sola voko. */
#define X48_KB_ROWS           9
#define X48_KB_MASK_ON   0x8000u

/* Bitoj de la indikiloj, en la maldekstra-al-dekstra ordo en kiu ili aperas
 * sur la vitro. */
#define X48_ANN_LEFT     0x0001u   /* maldekstra ŝovklavo */
#define X48_ANN_RIGHT    0x0002u   /* dekstra ŝovklavo    */
#define X48_ANN_ALPHA    0x0004u
#define X48_ANN_BATTERY  0x0008u
#define X48_ANN_BUSY     0x0010u
#define X48_ANN_IO       0x0020u

typedef struct x48_config_s {
    const char *rom_path;     /* deviga; NULL signifas "serĉu apud la stato" */

    /* Labortablo: dosierujo enhavanta ram / port1 / port2 / state.
     * Androida SAF: lasu NULL kaj transdonu anstataŭe antaŭmalfermitajn
     * priskribilojn, ĉar content://-arbadreso havas nenian POSIX-vojon por
     * doni. */
    const char *state_dir;
    int   fd_ram;             /* -1 kiam neuzata */
    int   fd_port1;
    int   fd_port2;
    int   fd_state;

    bool  read_only;          /* surmetu la kartpordojn nurlege */
    bool  throttle;           /* paŝu je vera HP 48-rapido, aŭ kuru libere */
} x48_config_t;

typedef struct x48_frame_s {
    /* Unu bajto por bildero, 0 aŭ 1. Indexed8 prefere ol pakitaj bitoj: 8,4 KB
     * estas nenio, kaj tio lasas QImage ĉirkaŭi la bufron sen bitmanipulado. */
    uint8_t  pixels[X48_LCD_PIXELS];
    int      width;           /* videbla larĝo, normale X48_LCD_WIDTH */
    int      height;          /* videbla alteco; 0 kiam la LCD estas malŝaltita */
    int      stride;
    int      contrast;        /* 0..31, kiel programita de la Saturn */
    uint16_t annunciators;    /* bitkampo X48_ANN_* */
} x48_frame_t;

/* --- vivociklo ---------------------------------------------------------- */

/* Ŝargas ROM-on + staton kaj ekfunkciigas la Saturn. Redonas false kaj metas
 * x48_last_error() je malbona aŭ manka ROM. */
bool        x48_init(const x48_config_t *cfg);
void        x48_shutdown(void);

/* Rulas ĝis max_cycles Saturn-ciklojn, redonas la nombron vere plenumitan. La
 * horloĝo de HP 48 estas ~4 MHz / ~2 MHz depende de la modelo, do 60 Hz-a
 * tiktako volas proksimume 70000 ciklojn. Redonas 0 se la procesoro estas
 * haltigita en profunda dormo. */
int         x48_run_slice(int max_cycles);

/* Vera se la procesoro estas en SHUTDN kaj nenio krom klavo aŭ horloĝo vekos
 * ĝin - la fasado tiam povas ĉesi tiktaki kaj lasi la aparaton dormi. */
bool        x48_is_asleep(void);

/* LA PROPRA RAPIDMEZURILO DE X48, kaj ĝi kostas nenion ĉar ĝi jam funkcias.
 * schedule() specimenas la realtempan horloĝon de la gastiganto ĉiujn 0x7ffff
 * instrukciojn - ĉirkaŭ okfoje sekunde - kaj tenas dek-specimenan glatigitan
 * nombron de la instrukcioj vere plenumitaj po reala sekundo, kiun ĝi uzas por
 * teni la propran horloĝon de la kalkulilo ĝusta, kiun ajn rapidon la
 * gastiganto havas. Ĉi tio nur legas ĝin. 0 antaŭ la unua specimeno kaj dum
 * nenio estas ŝargita.
 *
 * Ĝi respondas "kiom rapide ĉi tio kuras", kio estas la sola mezurebla flanko
 * de la rapid-demando: la kerno nombras INSTRUKCIOJN kaj neniam Saturn-ciklojn
 * (emulate.c:2216 estas la unu kaj sola nombrilo), do ĝi ne povas diri kiom
 * rapida estus vera 48. Tiu numero devas veni de ekstere kaj esti kalibrita. */
long        x48_instructions_per_second(void);
/* Ĉiu step_instruction() kiun ĉi tiu procezo rulis. Ne la propra nombrilo de
 * la kerno, kiun schedule() periode renulas. */
unsigned long long x48_instructions_total(void);

/* --- ekrano ------------------------------------------------------------- */

/* Kopias la nunan LCD-on en *out. Redonas false se nenio ŝanĝiĝis de la antaŭa
 * voko, por ke la vokanto povu tute preterlasi la teksturalŝuton. */
bool        x48_take_frame(x48_frame_t *out);

/* --- klavaro ------------------------------------------------------------ */

/* row estas 0..X48_KB_ROWS-1, mask estas la enira kolumnobito, aŭ
 * X48_KB_MASK_ON por ON (vico ignorata). Pluraj samtempaj premoj estas la tuta
 * celo: ON+A+F estas la malmola restarigo de HP 48 kaj devas alveni kiel tri
 * vivaj klavoj, ne kiel sinsekvo. */
void        x48_key_down(int row, uint16_t mask);
void        x48_key_up(int row, uint16_t mask);
void        x48_key_release_all(void);

/* --- stato -------------------------------------------------------------- */

void        x48_reset(bool cold);        /* cold == viŝu la RAM-on (ekvivalento de ON+A+F) */
bool        x48_save_state(void);
bool        x48_reload_state(void);      /* relegu post kiam ekstera sinkronigo skribis ĝin */

/* Fingropremo de la stato sur disko, por detekti "alia aparato skribis ĉi
 * tion". Malmultekosta: grando + mtime + haketo de la unua kaj lasta paĝo. */
uint64_t    x48_state_fingerprint(void);

/* Resumo de la RAM kiu estus skribita, por preterlasi konservon kiu ŝanĝus
 * nenion. 0 signifas "nenia opinio". Vidu la komenton ĉe la difino por tio,
 * kial la supra fingropremo ne uzeblas por ĉi tio. */
uint64_t    x48_ram_digest(void);

/* --- objektinterŝanĝo --------------------------------------------------- */

/* La duuma transiga formato de HP 48: "HPHP48-" plus revizia litero, poste la
 * objekto kiel krudaj duonbajtoj. Enporto puŝas sur stakan nivelon 1; elporto
 * skribas kion ajn estas sur nivelo 1, de kiu ajn tipo - la formato ne
 * zorgas. Ambaŭ redonas false kaj metas x48_last_error() je ĉia malsukceso,
 * inkluzive de dosiero kiu tute ne estas HP 48-objekto. */
bool        x48_stack_has_object(void);   /* ĉu io estas sur nivelo 1? */
bool        x48_import_file(const char *path);
/* La samaj kontroloj kiel x48_import_file() sen puŝi - inkluzive de libera
 * memoro, kiam kalkulilo funkcias. */
bool        x48_object_file_loadable(const char *path);
bool        x48_export_file(const char *path);

/* --- tondujo ------------------------------------------------------------ */

/* Bildigas nivelon 1 de la RPL-stako kiel UTF-8 en buf: reela nombro aŭ ĉeno,
 * kun la tuta HP 48-signaro tradukita al Unikodo. Redonas la skribitan
 * bajtolongon, aŭ la bezonatan longon (> buflen) se buf estis tro malgranda. */
size_t      x48_stack_to_text(char *buf, size_t buflen);

/* Puŝas UTF-8 sur la stakon: nombro iĝas reela nombro, ĉio alia ĉeno. Akceptas
 * Unikodon, la trigrafojn de la transiga formato (\<<, \->) kaj kaplinion
 * "%%HP: T(3)A(D)F(.);". Redonas false kaj nomas la signon kiam iu signo ne
 * havas HP 48-ekvivalenton. */
bool        x48_text_to_stack(const char *utf8);

/* La sama kontrolo sen puŝi: ĉu x48_text_to_stack() akceptus ĉi tiun tekston?
 * Ne bezonas funkciantan kalkulilon. */
bool        x48_text_loadable(const char *utf8);

bool        x48_level1_is_string(void);
uint32_t    x48_level1_address(void);     /* 0 kiam nivelo 1 estas malplena */

/* --- preteco por ricevi objekton ---------------------------------------- */

/* Kie la kalkulilo estas, legite rekte el ĝia RAM je la momento de la voko -
 * sen konservo, sen disko, en mikrosekundoj. Mezurita sur ĉiuj kvin
 * GX-revizioj K, L, M, P kaj R, kiuj konsentas duonbajton post duonbajto. */
typedef enum x48_readiness_e {
    X48_READY = 0,     /* ĉe la stako kaj senokupa: preta ricevi objekton   */
    X48_NOT_RUNNING,   /* neniu kalkulilo funkcias                          */
    X48_NOT_GX,        /* SX-ROM: la adresoj validas nur por la GX          */
    X48_BUSY,          /* programo kuras                                    */
    X48_OFF,           /* la kalkulilo estas malŝaltita                     */
    X48_EDITING,       /* komandlinio aŭ redaktilo malfermita               */
    X48_MESSAGE,       /* mesaĝo, ekzemple eraro, kovras la stakon          */
    X48_ELSEWHERE      /* formularo, aplikaĵo, interaga stako aŭ bildo      */
} x48_readiness_t;

x48_readiness_t x48_readiness(void);

/* Ĉu USER-reĝimo estas ŝaltita (1USR aŭ ŝlosita)? En ĝi ĉiu klavo povas havi
 * alian taskon, do la aŭtomata vojo ĉi-sube ne premas klavojn tiam. */
bool        x48_user_mode(void);

/* --- aŭtomataj →STR kaj STR→ -------------------------------------------- */

/* Puŝas malgrandan programon sur la stakon, por ke premo de EVAL rulu ĝin per
 * la ROM mem: « DUP →STR » lasas la originalon kaj ĝian tekston; « STR→ »
 * kompilas kaj plenumas ĉenon, ĝuste kiel tajpita komandlinio post ENTER. */
bool        x48_push_tostr_program(void);
bool        x48_push_strto_program(void);
bool        x48_drop_level1(void);

/* --- pepilo ------------------------------------------------------------- */

/* Nenula redono signifas ke la Saturn petis pepon post la lasta voko; la
 * fasado ludas ĝin. Konsumas la peton. */
bool        x48_take_beep(uint32_t *freq_hz, uint32_t *duration_ms);

/* --- diagnozo ----------------------------------------------------------- */

const char *x48_last_error(void);
const char *x48_core_version(void);      /* kiu forko/revizio estis enkorpigita */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* AGAPE48_X48_SHIM_H */
