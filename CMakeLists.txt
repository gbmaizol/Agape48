# -----------------------------------------------------------------------------
# Agape48 - HP 48 emulator: x48 C core, Qt 6 / QML frontend.
# "HP" in Brazilian Portuguese is "aga-pe" == Greek agape.
#
# Size is a first-class requirement. Every knob below is documented in
# README.md, section "Size budget". Defaults are the small ones.
# -----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

# Size-first default. Must be set before project() so the toolchain honours it.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "MinSizeRel;Debug" CACHE STRING "Configurations" FORCE)
endif()

project(Agape48
    VERSION 0.1.0
    DESCRIPTION "HP 48 emulator on the x48 core"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(AGAPE48_STATIC_QT               "Link a statically-built Qt into one self-contained binary" OFF)
option(AGAPE48_LTO                     "Link-time optimisation (biggest single size win)" ON)
option(AGAPE48_ICF                     "Identical code folding (needs lld or gold)" ON)
option(AGAPE48_QML_CACHEGEN            "Bake compiled QML bytecode in (faster start, larger binary)" OFF)
option(AGAPE48_CORE_OPTIMIZE_FOR_SPEED "Build the Saturn interpreter at -O2 instead of -Os" OFF)
option(AGAPE48_WEBP_PLUGIN             "Link Qt's qwebp image plugin (see README: WebP is NOT in QtGui)" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Agape48Size)   # defines the agape48::size INTERFACE target

# -----------------------------------------------------------------------------
# Qt - Core, Gui, Qml, Quick. Nothing else, ever. Enforced at the bottom.
# -----------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2)
qt_standard_project_setup(REQUIRES 6.5)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)   # all resources go through qt_add_qml_module
set(CMAKE_AUTOUIC OFF)   # no Widgets, no .ui files

add_subdirectory(src/core)

# -----------------------------------------------------------------------------
# Application
# -----------------------------------------------------------------------------
qt_add_executable(agape48
    src/main.cpp
    src/bridge/Agape48Engine.h      src/bridge/Agape48Engine.cpp
    src/bridge/LcdItem.h            src/bridge/LcdItem.cpp
    src/bridge/StateFileManager.h   src/bridge/StateFileManager.cpp
    src/bridge/SkinModel.h          src/bridge/SkinModel.cpp
)

# Windows subsystem, not console. Without this the linker produces a CUI binary
# and Windows opens an empty console window behind the calculator on every
# start - visible in the installed build, because a console is only suppressed
# when the program is launched from an existing one. qt_add_executable does not
# set this for you.
if(WIN32)
    set_target_properties(agape48 PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(agape48 PRIVATE src src/bridge src/core)
target_link_libraries(agape48 PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2
    agape48::x48core
    agape48::size
)

# Trim Qt's own footprint: drop deprecated APIs and the QString ASCII casts.
target_compile_definitions(agape48 PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=0x060500
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
    QT_USE_QSTRINGBUILDER
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_INFO_OUTPUT>
)

set(AGAPE48_QML_FILES
    qml/Main.qml
    qml/Calculator.qml
    qml/Keypad.qml
    qml/SettingsWindow.qml
    qml/SettingsContent.qml
    qml/SettingsPage.qml
    qml/KeyBindingWindow.qml
    qml/CalculatorPickerWindow.qml
    qml/HandoverWindow.qml
    qml/AdvancedWindow.qml
    qml/Agape48Keymap.qml
    qml/TextSizes.qml
)

set_source_files_properties(qml/Agape48Keymap.qml qml/TextSizes.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(agape48
    URI Agape48
    VERSION 1.0
    NO_PLUGIN                       # types are compiled straight into the exe
    QML_FILES ${AGAPE48_QML_FILES}
    RESOURCES
        assets/icon.png
        assets/skins/default/face.png
        assets/skins/default/layout.json
        assets/skins/default/ann_left.png
        assets/skins/default/ann_right.png
        assets/skins/default/ann_alpha.png
        assets/skins/default/ann_battery.png
        assets/skins/default/ann_busy.png
        assets/skins/default/ann_io.png
)

# Compiled-QML cache costs binary size and buys startup time. Off by default.
if(NOT AGAPE48_QML_CACHEGEN)
    set_source_files_properties(${AGAPE48_QML_FILES}
        PROPERTIES QT_QML_SKIP_CACHEGEN ON)
endif()

# -----------------------------------------------------------------------------
# Plugins - the single biggest static-link size lever after LTO.
# -----------------------------------------------------------------------------
if(AGAPE48_WEBP_PLUGIN)
    # QImage cannot read WebP out of the box: the qwebp plugin lives in the
    # separate qtimageformats module. Guarded so a Qt build without it still
    # configures - see README for the libwebp-direct alternative.
    if(TARGET Qt6::QWebpPlugin)
        qt_import_plugins(agape48 INCLUDE Qt6::QWebpPlugin)
    else()
        message(WARNING
            "AGAPE48_WEBP_PLUGIN=ON but Qt6::QWebpPlugin was not found. "
            "Install qtimageformats, or set AGAPE48_WEBP_PLUGIN=OFF and decode "
            "the skin with libwebp directly (see README).")
    endif()
endif()

qt_import_plugins(agape48
    EXCLUDE_BY_TYPE iconengines
    EXCLUDE_BY_TYPE qmltooling
    EXCLUDE_BY_TYPE networkinformation
    EXCLUDE_BY_TYPE tls
    EXCLUDE_BY_TYPE sqldrivers
    EXCLUDE_BY_TYPE printsupport
)

if(AGAPE48_STATIC_QT)
    qt_import_qml_plugins(agape48)   # required: pulls QtQuick's own QML plugins in
    if(NOT MSVC)
        target_link_options(agape48 PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

if(AGAPE48_LTO)
    set_target_properties(agape48 PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# -----------------------------------------------------------------------------
# Android
# -----------------------------------------------------------------------------
if(ANDROID)
    # One ABI only. Multi-ABI roughly multiplies the .so payload.
    set(QT_ANDROID_ABIS "arm64-v8a" CACHE STRING "Android ABIs to package" FORCE)

    set_target_properties(agape48 PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android"
        QT_ANDROID_MIN_SDK_VERSION    26
        QT_ANDROID_TARGET_SDK_VERSION 35
        # Compiled against 36, still TARGETING 35, and the two are different
        # questions: compileSdk only says which APIs the code may name, while
        # targetSdk is what opts the app in to a platform's new runtime
        # behaviour. Qt 6.12's gradle template depends on androidx.core 1.17.0,
        # which refuses to be compiled against anything below 36; nothing about
        # this app wants Android 16's behaviour changes yet.
        QT_ANDROID_COMPILE_SDK_VERSION 36
        # dk.geeak.agape48, not org.qtproject.example.agape48: the Java in
        # platform/android/src has always been in that package, and SafBridge is
        # reached by that name from StateFileManager.cpp.
        QT_ANDROID_PACKAGE_NAME       "dk.geeak.agape48"
        # Without this the launcher shows a blank icon - measured on the first
        # APK. The drawable is assets/icon.png, copied into the package source
        # dir because res/ is what aapt reads.
        QT_ANDROID_APP_ICON           "@drawable/icon"
        QT_ANDROID_VERSION_NAME       "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE       1
        QT_ANDROID_APP_NAME           "Agape48"
    )
    # androiddeployqt strips the .so itself; --exclude-libs keeps static-lib
    # symbols out of the dynamic table on top of that.
    target_link_options(agape48 PRIVATE -Wl,--exclude-libs,ALL)
endif()

# -----------------------------------------------------------------------------
# Desktop install + strip
# -----------------------------------------------------------------------------
if(NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS agape48 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    qt_generate_deploy_qml_app_script(
        TARGET agape48
        OUTPUT_SCRIPT AGAPE48_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
        MACOS_BUNDLE_POST_BUILD)
    install(SCRIPT ${AGAPE48_DEPLOY_SCRIPT})
endif()

# -----------------------------------------------------------------------------
# Main.qml imports the QtCore QML module for Settings (window geometry, kept
# machine-local by design - see docs/design-questions.md items 10b and 11). It
# is backed by Qt6::Core, which is already linked, so nothing new is added here.
# On a STATIC Qt build its QML plugin still has to be imported; Qt's CMake does
# that automatically for an app with its own qt_add_qml_module, but it is the
# first thing to check if a static build dies with "module QtCore is not
# installed".

# Guard: fail the configure step if a forbidden Qt module gets linked.
# Catches direct links only; `ldd`/`readelf -d` is still the final word.
# -----------------------------------------------------------------------------
set(AGAPE48_FORBIDDEN
    Qt6::Network Qt6::Multimedia Qt6::MultimediaQuick Qt6::Sql Qt6::Widgets
    Qt6::WebEngineQuick Qt6::WebSockets Qt6::Charts Qt6::3DCore
    Qt6::PrintSupport Qt6::Svg Qt6::Concurrent)
get_target_property(_a48_links agape48 LINK_LIBRARIES)
foreach(_mod IN LISTS AGAPE48_FORBIDDEN)
    if("${_mod}" IN_LIST _a48_links)
        message(FATAL_ERROR "Agape48 links ${_mod}. Lean-module rule violated.")
    endif()
endforeach()

message(STATUS "")
message(STATUS "Agape48 ${PROJECT_VERSION}")
message(STATUS "  build type ......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt ................. ${Qt6_VERSION} (${CMAKE_PREFIX_PATH})")
message(STATUS "  static Qt .......... ${AGAPE48_STATIC_QT}")
message(STATUS "  LTO / ICF .......... ${AGAPE48_LTO} / ${AGAPE48_ICF}")
message(STATUS "  QML cachegen ....... ${AGAPE48_QML_CACHEGEN}")
message(STATUS "  core at -O2 ........ ${AGAPE48_CORE_OPTIMIZE_FOR_SPEED}")
message(STATUS "")

target_compile_definitions(agape48 PRIVATE AGAPE48_VERSION="${PROJECT_VERSION}")
