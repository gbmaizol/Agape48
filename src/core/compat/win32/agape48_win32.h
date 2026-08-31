/* agape48_win32.h - force-included into every vendored core translation unit on
 * Windows (gcc -include, MSVC /FI), so src/core/x48/ needs no edits and a
 * re-vendor cannot silently revert this. Same principle as the X11 no-ops in
 * x48_shim.c: absorb the difference in the seam, not in files that get replaced.
 *
 * One thing lives here.
 *
 * POSIX mkdir(path, mode) takes a mode. The Windows CRT's takes only a path.
 * init.c:1668 calls the two-argument form in write_files(), which is live code -
 * it creates the state folder when it does not exist yet.
 *
 * The ordering below is the whole trick and is not optional. MinGW declares
 * `int mkdir(const char *)` as a real function - not a macro - in <io.h>:282 and
 * <direct.h>:65. A two-argument function-like macro named mkdir that is already
 * in scope when the compiler reaches those declarations is a hard error, because
 * a macro invoked with the wrong argument count is not merely left alone. So
 * both headers are pulled in FIRST, while mkdir is still an ordinary identifier;
 * their include guards then make the core's own includes of them no-ops, and
 * nothing declares mkdir again after the macro exists.
 *
 * If a future toolchain declares mkdir somewhere other than these two headers,
 * that shows up as "macro mkdir requires 2 arguments" and the fix is to add the
 * header to the list below.
 */
#ifndef AGAPE48_WIN32_H
#define AGAPE48_WIN32_H

#ifdef _WIN32

#include <io.h>       /* declares the CRT's one-argument mkdir */
#include <direct.h>   /* declares _mkdir, and mkdir again */

#undef mkdir
#define mkdir(path, mode) _mkdir(path)

#endif /* _WIN32 */

#endif /* AGAPE48_WIN32_H */
