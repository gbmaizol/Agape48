# -----------------------------------------------------------------------------
# agape48::size - INTERFACE-celo portanta ĉiun grandon-reduktan flagon kiun la
# aktiva ilaro vere akceptas. La flagoj estas sonditaj, neniam supozitaj, por ke
# la sama arbo konfiguriĝu sur gcc, clang, la clang de la Androida NDK kaj MSVC.
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
    # --- kodgenerado -----------------------------------------------------
    # -Os estas metita de CMAKE_*_FLAGS_MINSIZEREL; la restaĵo estas tio, kion
    # MinSizeRel ne donas al vi.
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

    # La kapdosieroj de Qt uzas throw/catch kaj typeid en kelkaj lokoj, do
    # esceptoj kaj RTTI restas ŜALTITAJ. Malŝalti ilin pagas nur se vi ankaŭ
    # rekonstruas Qt per -no-exceptions -no-rtti; vidu Readme_Programmers.md
    # "Size budget".
    a48_try_compile_flag(-fasynchronous-unwind-tables)

    # --- ligado ----------------------------------------------------------
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
        # Kunfaldo de identa kodo: nur lld kaj gold realigas ĝin. Kutime 3-6%
        # for de Qt Quick-duumaĵo, pro ŝablonaj ekzempligoj.
        a48_try_link_flag(-Wl,--icf=all)
    endif()

    # Senigu je simboloj en optimumigitaj konstruoj - sed ne sur Androido, kie
    # androiddeployqt volas la nesenigitan .so por unue produkti simboldosieron.
    if(NOT ANDROID)
        target_link_options(agape48_size INTERFACE
            "$<$<OR:$<CONFIG:MinSizeRel>,$<CONFIG:Release>>:-Wl,-s>")
    endif()
endif()
