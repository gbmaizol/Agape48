# Kiu konstruo estas ĉi tiu? Skribas agape48_build.h, kiun la fenestro "About"
# montras, kaj kiun neniu homo devas tajpi.
#
# Provo 17 linio 4: la fenestro "About" portas la titolon, la version, la
# konstrunumeron kaj tempindikon kiun la konstruprocezo mem faras.
#
# LA TEMPINDIKO ESTAS TIU DE LA ENARBIGO, NE DE LA LIGADO, kaj tio estas elekto
# anstataŭ oportuno. Horloĝa tempo ĉi tie ŝanĝiĝus je ĈIU konstruo, do la
# kaptilo ŝanĝiĝus je ĉiu konstruo, do agape48 relegiĝus je ĉiu konstruo - kaj
# ĉi tiu projekto ligas kun LTO, kie tio kostas dekojn da sekundoj ĉiufoje kiam
# oni ŝanĝas unu QML-dosieron. La enarbiga tempo respondas la demandon kiun li
# efektive havas - "ĉu ĉi tiu duumaĵo enhavas la riparon kiun mi petis" - kaj ĝi
# ŝanĝiĝas ekzakte kiam la fonto ŝanĝiĝas.
#
# Lanĉata per -P el propra celo, do ĝi kuras je konstruotempo kaj ne je
# agordotempo: string(TIMESTAMP) en CMakeLists.txt frostiĝus ĉe la tago kiam
# oni lastfoje kuris cmake.
#
#   cmake -DOUT=<dosiero> -DSRC=<arbo> -P cmake/BuildStamp.cmake

set(commit "")
set(stamp "")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
        WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cd --date=format:%Y%b%d-%Hh%M
        WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE stamp
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # Ĉu la arbo portas ŝanĝojn kiujn neniu enarbigis. Sen tio ĉi, konstruo el
    # duone redaktita arbo mensogus per la hash de la lasta enarbigo.
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE dirty
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT dirty STREQUAL "")
        set(commit "${commit}+")
    endif()
endif()

string(TOLOWER "${stamp}" stamp)
if(commit STREQUAL "")
    # Tarbalo anstataŭ klono, aŭ git ne instalita. La versinumero sole ankoraŭ
    # estas vera, do la fenestro montras ĝin kaj silentas pri la cetero.
    set(build "")
elseif(stamp STREQUAL "")
    set(build "${commit}")
else()
    set(build "${commit} · ${stamp}")
endif()

set(text "// Generita de cmake/BuildStamp.cmake. Ne redaktu.\n#pragma once\n#define AGAPE48_BUILD \"${build}\"\n")

# Skribi nur kiam la teksto ŝanĝiĝis: alie ĉi tiu celo tuŝus la kaptilon je ĉiu
# konstruo kaj la LTO-ligado kurus denove por nenio.
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUT}" "${text}")
    message(STATUS "agape48: build stamp = ${build}")
endif()
