/* agape48_win32.h - deviga inkluzivo en ĉiun tradukunuon de la enkorpigita
 * kerno sub Vindozo (gcc -include, MSVC /FI), por ke src/core/x48/ bezonu
 * nenian redakton kaj por ke reenkorpigo ne povu silente malfari ĉi tion. Sama
 * principo kiel la X11-nuloperacioj en x48_shim.c: absorbu la diferencon en la
 * kudro, ne en dosieroj kiuj estos anstataŭigitaj.
 *
 * Unu afero vivas ĉi tie.
 *
 * POSIX-a mkdir(vojo, modo) prenas modon. Tiu de la Vindoza CRT prenas nur
 * vojon. init.c:1668 vokas la duargumentan formon en write_files(), kio estas
 * viva kodo - ĝi kreas la statan dosierujon kiam tiu ankoraŭ ne ekzistas.
 *
 * La suba ordo estas la tuta lertaĵo kaj ne estas laŭvola. MinGW deklaras
 * `int mkdir(const char *)` kiel veran funkcion - ne makroon - en <io.h>:282
 * kaj <direct.h>:65. Duargumenta funkcisimila makroo nomata mkdir, jam videbla
 * kiam la tradukilo atingas tiujn deklarojn, estas malmola eraro, ĉar makroo
 * vokita kun malĝusta nombro da argumentoj ne estas simple lasita trankvila.
 * Do ambaŭ kapdosieroj estas altiritaj UNUE, dum mkdir ankoraŭ estas ordinara
 * identigilo; iliaj inkluzivgardiloj poste igas nuloperacioj la proprajn
 * inkluzivojn de la kerno, kaj nenio deklaras mkdir denove post kiam la makroo
 * ekzistas.
 *
 * Se estonta ilaro deklaros mkdir ie ajn krom en tiuj du kapdosieroj, tio
 * aperos kiel "macro mkdir requires 2 arguments" kaj la solvo estas aldoni la
 * kapdosieron al la suba listo.
 */
#ifndef AGAPE48_WIN32_H
#define AGAPE48_WIN32_H

#ifdef _WIN32

#include <io.h>       /* deklaras la unuargumentan mkdir de la CRT */
#include <direct.h>   /* deklaras _mkdir, kaj mkdir denove */

#undef mkdir
#define mkdir(path, mode) _mkdir(path)

#endif /* _WIN32 */

#endif /* AGAPE48_WIN32_H */
