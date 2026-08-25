# -----------------------------------------------------------------------------
# agape48::size - an INTERFACE target carrying every size-reducing flag that the
# active toolchain actually accepts. Flags are probed, never assumed, so the
# same tree configures on gcc, clang, Android NDK clang and MSVC.
# -----------------------------------------------------------------------------
include_guard(GLOBAL)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

add_library(agape48_size INTERFACE)
add_library(agape48::size ALIAS agape48_size)

function(a48_try_compile_flag flag)
    string(MAKE_C_IDENTIFIER "A48_CFLAG_${flag}" _var)
    check_c_compiler_flag("${flag}" ${_var})
    if(${_var})
        target_compile_options(agape48_size INTERFACE "${flag}")
    endif()
endfunction()

function(a48_try_cxx_flag flag)
    string(MAKE_C_IDENTIFIER "A48_CXXFLAG_${flag}" _var)
    check_cxx_compiler_flag("${flag}" ${_var})
    if(${_var})
        target_compile_options(agape48_size INTERFACE
            "$<$<COMPILE_LANGUAGE:CXX>:${flag}>")
    endif()
endfunction()

function(a48_try_link_flag flag)
    string(MAKE_C_IDENTIFIER "A48_LDFLAG_${flag}" _var)
    check_linker_flag(CXX "${flag}" ${_var})
    if(${_var})
        target_link_options(agape48_size INTERFACE "${flag}")
    endif()
endfunction()

if(MSVC)
    target_compile_options(agape48_size INTERFACE
        /O1          # favour size
        /Gy          # function-level linking, lets /OPT:REF drop dead code
        /Gw          # same for data
        /GS-         # no stack cookies
        /Zc:inline   # drop unreferenced COMDATs
        /utf-8
    )
    target_link_options(agape48_size INTERFACE
        /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE
    )
else()
    # --- codegen ---------------------------------------------------------
    # -Os is set by CMAKE_*_FLAGS_MINSIZEREL; the rest is what MinSizeRel
    # does not give you.
    foreach(_f
        -ffunction-sections          # let --gc-sections work per function
        -fdata-sections
        -fvisibility=hidden          # smaller dynsym, better LTO/ICF
        -fmerge-all-constants
        -fno-plt
        -fno-semantic-interposition
        -fno-asynchronous-unwind-tables   # dropped again below if EH is on
        -fno-ident
        -fno-math-errno
    )
        a48_try_compile_flag(${_f})
    endforeach()

    a48_try_cxx_flag(-fvisibility-inlines-hidden)

    # Qt's headers use throw/catch and typeid in places, so exceptions and RTTI
    # stay ON. Turning them off only pays if you also rebuild Qt with
    # -no-exceptions -no-rtti; see README "Size budget".
    a48_try_compile_flag(-fasynchronous-unwind-tables)

    # --- link ------------------------------------------------------------
    foreach(_f
        -Wl,--gc-sections
        -Wl,--as-needed
        -Wl,-O2
        -Wl,--build-id=none
        -Wl,--no-eh-frame-hdr
    )
        a48_try_link_flag(${_f})
    endforeach()

    if(AGAPE48_ICF)
        # Identical code folding: only lld and gold implement it. Typically
        # 3-6% off a Qt Quick binary because of template instantiations.
        a48_try_link_flag(-Wl,--icf=all)
    endif()

    # Strip in optimised builds - but not on Android, where androiddeployqt
    # wants the unstripped .so to produce a symbol file first.
    if(NOT ANDROID)
        target_link_options(agape48_size INTERFACE
            "$<$<OR:$<CONFIG:MinSizeRel>,$<CONFIG:Release>>:-Wl,-s>")
    endif()
endif()
