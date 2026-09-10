/* pwd.h - Vindozo ne havas pasvortan datumbazon.
 *
 * La enkorpigita arbo inkluzivas <pwd.h> en ekzakte unu loko: init.c, por la
 * retrofalo getpwuid(getuid()) de get_home_directory(). Tiu funkcio estas
 * mortinta kodo. Ĝia sola vokloko, init.c:1188, estas komentita for - Droid48
 * faris tion - kaj `homeDirectory` ankaŭ neniam ricevas valoron (resources.c:91
 * estas same komentita for), do la funkcio malreferencus nulan montrilon se io
 * vokus ĝin. -ffunction-sections kun --gc-sections forĵetas ĝin. Ĝi devas nur
 * kompiliĝi.
 *
 * NULL estas ankaŭ la honesta respondo sub Vindozo, prefere ol la blanka
 * mensogo de stumpo: ne ekzistas pasvorta datumbazo por konsulti. La vokanto
 * jam traktas NULL - ĝi retrofalas al $HOME, poste al "/tmp".
 *
 * Ĉi tio vivas ekster src/core/x48/ intence, por ke reenkorpigo de la kerno
 * neniam devu scii ke ĝi ekzistas. Sama kialo pro kiu la X11-nuloperacioj
 * vivas en x48_shim.c anstataŭ esti forredaktitaj el dosieroj kiuj eble estos
 * reenkorpigitaj.
 */
#ifndef AGAPE48_COMPAT_PWD_H
#define AGAPE48_COMPAT_PWD_H

struct passwd {
    char *pw_name;
    char *pw_dir;
    char *pw_shell;
};

static inline struct passwd *getpwuid(int uid) { (void)uid; return NULL; }

/* La <unistd.h> de MinGW ne deklaras getuid(), kvankam la CRT solvas la
 * simbolon. Deklari ĝin ĉi tie tenas la kernon for de implica deklaro, kio
 * estas averto sur GCC 13 kaj eraro de GCC 14 ekde. */
static inline int getuid(void) { return 0; }

#endif /* AGAPE48_COMPAT_PWD_H */
