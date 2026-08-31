/* pwd.h - Windows has no passwd database.
 *
 * The vendored tree includes <pwd.h> in exactly one place: init.c, for
 * get_home_directory()'s getpwuid(getuid()) fallback. That function is dead
 * code. Its only call site, init.c:1188, is commented out - Droid48 did that -
 * and `homeDirectory` is never assigned either (resources.c:91 is commented out
 * too), so the function would dereference a null pointer if anything did call
 * it. -ffunction-sections with --gc-sections drops it. It only has to compile.
 *
 * NULL is also the honest answer on Windows rather than a stub's white lie:
 * there is no passwd database to consult. The caller already handles NULL - it
 * falls back to $HOME, then to "/tmp".
 *
 * This lives outside src/core/x48/ on purpose, so re-vendoring the core never
 * has to know it exists. Same reason the X11 no-ops live in x48_shim.c instead
 * of being edited out of files that may be re-vendored.
 */
#ifndef AGAPE48_COMPAT_PWD_H
#define AGAPE48_COMPAT_PWD_H

struct passwd {
    char *pw_name;
    char *pw_dir;
    char *pw_shell;
};

static inline struct passwd *getpwuid(int uid) { (void)uid; return NULL; }

/* MinGW's <unistd.h> does not declare getuid(), although the CRT resolves the
 * symbol. Declaring it here keeps the core off an implicit declaration, which
 * is a warning on GCC 13 and an error from GCC 14 on. */
static inline int getuid(void) { return 0; }

#endif /* AGAPE48_COMPAT_PWD_H */
