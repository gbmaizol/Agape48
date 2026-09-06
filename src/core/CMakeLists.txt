# -----------------------------------------------------------------------------
# agape48::x48core - the vendored x48 Saturn emulator plus the shim the Qt side
# talks to. See VENDORING.md before touching this file.
# -----------------------------------------------------------------------------

# Upstream x48 sources go in src/core/x48/ (git submodule or a plain copy).
# Listed explicitly rather than globbed so a vendor bump is a visible diff.
set(X48_SOURCES
    x48/emulate.c       # Saturn interpreter + step_instruction + agape48 slice
    x48/actions.c       # opcodes with side effects, incl. do_shutdown
    x48/memory.c
    x48/register.c
    x48/timer.c
    x48/device.c
    x48/init.c          # init_emulator / init_active_stuff
    x48/romio.c         # ROM and state file I/O
    x48/lcd.c           # writes lcd_buffer; x48_take_frame reads it
    x48/errors.c
    x48/rpl.c           # only real_number() survives, for the clipboard
    x48/binio.c         # read_bin_file, for object import
    x48/resources.c     # config defaults; no X11 left in it
    x48/options.c

    # Deliberately NOT built:
    #   x48/main.c      JNI entry points + main(); the shim replaces both, and
    #                   not compiling it is what frees blockConditionVariable()
    #                   for the shim to define. See x48_shim.c.
    #   x48/x48.c       X11 frontend
    #   x48/debugger.c  interactive debugger; rpl.c is kept without it
    #   x48/disasm.c    only the debugger needs it
    #   x48/serial.c    Droid48 has no serial either - it compiles this file
    #                   only because it inherited x48's source list whole.
    #                   useTerminal and useSerial are hardcoded to 0 in
    #                   resources.c:84, the one thing that could raise them is
    #                   X48.ad.h (x48's X11 app-defaults table, unparsed once
    #                   the X11 frontend is gone), and there is no serial,
    #                   Kermit or XModem anywhere in Droid48's Java. So it is
    #                   already dead at runtime; dropping it changes nothing,
    #                   matches decision 2c, and is what lets this build for
    #                   MSVC and MinGW, which have no termios.h, ptsname_r,
    #                   grantpt, unlockpt or O_NDELAY. Its four entry points
    #                   are no-ops in x48_shim.c.
)

add_library(agape48_x48core STATIC
    x48_shim.h
    x48_shim.c
    ${X48_SOURCES}
)
add_library(agape48::x48core ALIAS agape48_x48core)

target_include_directories(agape48_x48core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/x48/src)

target_compile_definitions(agape48_x48core PRIVATE
    # x48 upstream is an X11 program. These keep the X11/curses/GUI paths out.
    X48_HEADLESS_CORE=1
    _GNU_SOURCE
)

# Windows lacks two POSIX things the core reaches for. Both are absorbed here
# rather than by editing the vendored tree, so a re-vendor never has to know.
#
#   compat/win32  supplies <pwd.h>; that file explains why NULL is safe and why
#                 the function needing it is already dead.
#   SYSV_TIME     picks timer.c:212's other branch, which reads the global
#                 `timezone` instead of ltm->tm_gmtoff. This is not a Windows
#                 workaround: global.h:79 already defines SYSV_TIME whenever
#                 `linux` is defined, so tm_gmtoff is dead on Linux too and this
#                 puts Windows on the exact path Linux has always used. MinGW's
#                 CRT has `timezone` with the same sign convention - seconds
#                 west of UTC - including the DST hour the branch subtracts.
if(WIN32)
    target_include_directories(agape48_x48core
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat/win32)
    target_compile_definitions(agape48_x48core PRIVATE SYSV_TIME=1)

    # mkdir's arity differs, and the fix has to be in scope before the CRT
    # headers declare mkdir. A force-include is the only place that is. The
    # header itself explains the ordering.
    set(_a48_win32_compat
        "${CMAKE_CURRENT_SOURCE_DIR}/compat/win32/agape48_win32.h")
    if(MSVC)
        target_compile_options(agape48_x48core PRIVATE "/FI${_a48_win32_compat}")
    else()
        target_compile_options(agape48_x48core PRIVATE -include "${_a48_win32_compat}")
    endif()
endif()

target_link_libraries(agape48_x48core PRIVATE agape48::size)

# The Saturn interpreter is the one hot loop in the whole program. -Os on an
# interpreter dispatch switch can cost real throughput; this escape hatch keeps
# the rest of the binary at -Os while the core gets -O2.
if(AGAPE48_CORE_OPTIMIZE_FOR_SPEED AND NOT MSVC)
    target_compile_options(agape48_x48core PRIVATE -O2)
endif()

# x48 is decades-old C. Do not let its warnings drown ours.
#
# The last one is not a matter of taste: the Android NDK's clang adds
# -Werror=format-security of its own accord, and rpl.c trips it six times with
#
#     sprintf(p, hp48_trans_tbl[c].trans);
#
# which is a table lookup where a literal is expected. Absorbed here rather than
# in the vendored file, the same way Windows' two POSIX gaps are - see
# VENDORING.md. It is latent rather than live: every "trans" string in
# hp48char.h is an HP 48 character name and not one of them contains a percent
# sign, so nothing today reaches the undefined behaviour. If one ever does, the
# fix is "%s" at all six sites and not this line.
if(NOT MSVC)
    target_compile_options(agape48_x48core PRIVATE
        -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough
        -Wno-error=format-security)
endif()
