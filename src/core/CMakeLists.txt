# -----------------------------------------------------------------------------
# agape48::x48core - the vendored x48 Saturn emulator plus the shim the Qt side
# talks to. See VENDORING.md before touching this file.
# -----------------------------------------------------------------------------

# Upstream x48 sources go in src/core/x48/ (git submodule or a plain copy).
# Listed explicitly rather than globbed so a vendor bump is a visible diff.
set(X48_SOURCES
    # x48/src/emulate.c
    # x48/src/cpu.c
    # x48/src/memory.c
    # x48/src/register.c
    # x48/src/timer.c
    # x48/src/romio.c
    # x48/src/lcd.c
    # x48/src/rpl.c
    # x48/src/serial.c        <- see VENDORING.md: stub this out, saves ~8 KB
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

target_link_libraries(agape48_x48core PRIVATE agape48::size)

# The Saturn interpreter is the one hot loop in the whole program. -Os on an
# interpreter dispatch switch can cost real throughput; this escape hatch keeps
# the rest of the binary at -Os while the core gets -O2.
if(AGAPE48_CORE_OPTIMIZE_FOR_SPEED AND NOT MSVC)
    target_compile_options(agape48_x48core PRIVATE -O2)
endif()

# x48 is decades-old C. Do not let its warnings drown ours.
if(NOT MSVC)
    target_compile_options(agape48_x48core PRIVATE
        -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough)
endif()
