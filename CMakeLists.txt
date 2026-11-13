# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.21)
project(CalcA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick LinguistTools Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QALCULATE REQUIRED IMPORTED_TARGET libqalculate)

qt_standard_project_setup(REQUIRES 6.8 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES de)

set(APP_ID io.github.arekown.CalcA)

# Engine + QML module as a static library, shared by the app and the tests.
qt_add_library(calca_core STATIC)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(calca_core
    URI CalcA
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/CalcA
    QML_FILES
        qml/Main.qml
        qml/Controller.qml
        qml/Key.qml
        qml/BarButton.qml
        qml/IconButton.qml
        qml/Theme.qml
        qml/logic.js
    SOURCES
        src/qalcengine.h
        src/qalcengine.cpp
    RESOURCES
        fonts/Orbitron-Variable.ttf
        fonts/Rajdhani-Medium.ttf
        fonts/Rajdhani-SemiBold.ttf
        fonts/Rajdhani-Bold.ttf
        fonts/ShareTechMono-Regular.ttf
        icons/${APP_ID}.svg
)
target_include_directories(calca_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(calca_core PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick PkgConfig::QALCULATE)

qt_add_executable(calca src/main.cpp)
target_compile_definitions(calca PRIVATE CALCA_VERSION="${PROJECT_VERSION}")
target_link_libraries(calca PRIVATE calca_core calca_coreplugin)

qt_add_translations(calca
    SOURCE_TARGETS calca_core
    TS_FILE_BASE calca
    TS_FILE_DIR i18n
    RESOURCE_PREFIX /i18n
)

# Tests
enable_testing()
qt_add_executable(tst_calc tests/tst_calc.cpp)
target_link_libraries(tst_calc PRIVATE calca_core calca_coreplugin Qt6::Test)
add_test(NAME tst_calc COMMAND tst_calc)
set_tests_properties(tst_calc PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Install: binary, menu entry, icon
install(TARGETS calca RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(data/${APP_ID}.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES icons/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
