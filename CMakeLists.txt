cmake_minimum_required(VERSION 3.16)

project(hebocr VERSION 0.12.0 LANGUAGES C CXX)

option(HEBOCR_WITH_TIFF "Read black-and-white TIFF files with libtiff" ON)
option(HEBOCR_WITH_FFTW "Use FFTW for image filtering" OFF)
option(HEBOCR_BUILD_CLI "Build the hebocr command-line tool (needs GLib)" ON)
option(HEBOCR_BUILD_GUI "Build the hebocr-gui graphical front end (needs Qt 6)" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

string(TIMESTAMP HEBOCR_BUILD_DATE "%Y%m%d")

# Build identification shown next to the version, e.g. "build 42, a1b2c3d".
# GitHub Actions numbers its runs; local builds say "local build".
find_package(Git QUIET)
set(HEBOCR_COMMIT "")
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE HEBOCR_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(DEFINED ENV{GITHUB_RUN_NUMBER})
  set(HEBOCR_BUILD_LABEL "build $ENV{GITHUB_RUN_NUMBER}")
else()
  set(HEBOCR_BUILD_LABEL "local build")
endif()
if(HEBOCR_COMMIT)
  string(APPEND HEBOCR_BUILD_LABEL ", ${HEBOCR_COMMIT}")
endif()
message(STATUS "hebOCR ${PROJECT_VERSION} (${HEBOCR_BUILD_LABEL})")
set(HEBOCR_BUILD_STR "${HEBOCR_BUILD_LABEL}, ${HEBOCR_BUILD_DATE}")

# ---------------------------------------------------------------------------
# libhebocr: the recognition engine
# ---------------------------------------------------------------------------
add_library(libhebocr STATIC
  src/hocr.c
  src/ho_pixbuf.c
  src/ho_array.c
  src/ho_array_hist.c
  src/ho_array_stat.c
  src/ho_bitmap.c
  src/ho_bitmap_hist.c
  src/ho_objmap.c
  src/ho_dimentions.c
  src/ho_segment.c
  src/ho_font.c
  src/ho_recognize.c
  src/ho_recognize_nikud.c
  src/fonts/ho_recognize_font_1.c
  src/fonts/ho_recognize_font_2.c
  src/fonts/ho_recognize_font_3.c
  src/fonts/ho_recognize_font_4.c
  src/fonts/ho_recognize_font_5.c
  src/fonts/ho_recognize_font_6.c
  src/ho_string.c
  src/ho_linguistics.c
  src/ho_layout.c
)
set_target_properties(libhebocr PROPERTIES OUTPUT_NAME hebocr)
target_include_directories(libhebocr PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts)
target_compile_definitions(libhebocr PUBLIC
  USE_STB_IMAGE
  VERSION="${PROJECT_VERSION}"
  BUILD="${HEBOCR_BUILD_STR}")

if(MSVC)
  target_compile_definitions(libhebocr PUBLIC _CRT_SECURE_NO_WARNINGS)
  # source files contain UTF-8 Hebrew string literals
  target_compile_options(libhebocr PUBLIC /utf-8)
else()
  target_link_libraries(libhebocr PUBLIC m)
endif()

if(HEBOCR_WITH_TIFF)
  find_package(TIFF)
  if(TIFF_FOUND)
    target_compile_definitions(libhebocr PUBLIC USE_TIFF)
    target_link_libraries(libhebocr PUBLIC TIFF::TIFF)
  else()
    message(STATUS "libtiff not found; building without TIFF support")
  endif()
endif()

if(HEBOCR_WITH_FFTW)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
  target_compile_definitions(libhebocr PUBLIC USE_FFTW)
  target_link_libraries(libhebocr PUBLIC PkgConfig::FFTW3)
endif()

# ---------------------------------------------------------------------------
# hebocr: command-line tool
# ---------------------------------------------------------------------------
if(HEBOCR_BUILD_CLI)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)

  add_executable(hebocr examples/hocr/hebocr.c)
  target_compile_definitions(hebocr PRIVATE
    PACKAGE_NAME="${PROJECT_NAME}"
    PACKAGE_STRING="${PROJECT_NAME} ${PROJECT_VERSION}")
  target_link_libraries(hebocr PRIVATE libhebocr PkgConfig::GLIB2)

  install(TARGETS hebocr RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# hebocr-gui: graphical front end (runs the hebocr CLI as a subprocess)
# ---------------------------------------------------------------------------
if(HEBOCR_BUILD_GUI AND HEBOCR_BUILD_CLI)
  find_package(Qt6 6.4 COMPONENTS Widgets)
  if(Qt6_FOUND)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    qt_standard_project_setup()
    set(HEBOCR_GUI_WIN_RC "")
    if(WIN32)
      set(HEBOCR_GUI_WIN_RC gui/hebocr.rc)
    endif()
    qt_add_executable(hebocr-gui WIN32
      ${HEBOCR_GUI_WIN_RC}
      gui/main.cpp
      gui/mainwindow.cpp
      gui/mainwindow.h
      gui/guidewindow.cpp
      gui/guidewindow.h)
    target_compile_definitions(hebocr-gui PRIVATE
      HEBOCR_VERSION="${PROJECT_VERSION}"
      HEBOCR_BUILD_LABEL="${HEBOCR_BUILD_LABEL}")
    target_link_libraries(hebocr-gui PRIVATE Qt6::Widgets)
    qt_add_resources(hebocr-gui "docs"
      PREFIX "/"
      FILES docs/options.md docs/third-party.md)
    qt_add_resources(hebocr-gui "icons"
      PREFIX "/"
      BASE gui
      FILES gui/icons/hebocr-16.png gui/icons/hebocr-32.png gui/icons/hebocr-48.png
            gui/icons/hebocr-64.png gui/icons/hebocr-128.png gui/icons/hebocr-256.png)
    install(TARGETS hebocr-gui RUNTIME DESTINATION bin)
    if(UNIX AND NOT APPLE)
      install(FILES gui/hebocr.desktop DESTINATION share/applications)
      foreach(size 16 32 48 64 128 256 512)
        install(FILES gui/icons/hebocr-${size}.png
          DESTINATION share/icons/hicolor/${size}x${size}/apps
          RENAME hebocr.png)
      endforeach()
    endif()

    # smoke test: open a page, run OCR through the GUI, require some text
    # (headless; Windows CI can't create the offscreen display reliably)
    find_package(Qt6 COMPONENTS Test)
    if(Qt6Test_FOUND AND NOT WIN32)
      qt_add_executable(hebocr-gui-tests
        gui/tests/test_gui.cpp
        gui/mainwindow.cpp
        gui/mainwindow.h
        gui/guidewindow.cpp
        gui/guidewindow.h)
      target_include_directories(hebocr-gui-tests PRIVATE gui)
      target_compile_definitions(hebocr-gui-tests PRIVATE
        HEBOCR_VERSION="${PROJECT_VERSION}"
        HEBOCR_BUILD_LABEL="${HEBOCR_BUILD_LABEL}"
        TEST_IMAGE="${CMAKE_CURRENT_SOURCE_DIR}/tests/images/genesis_nikud.png")
      target_link_libraries(hebocr-gui-tests PRIVATE Qt6::Widgets Qt6::Test)
      qt_add_resources(hebocr-gui-tests "docs_tests"
        PREFIX "/"
        FILES docs/options.md docs/third-party.md)
      add_test(NAME gui_tests COMMAND hebocr-gui-tests)
      set_tests_properties(gui_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/gui_tests_config")
    endif()

    if(NOT WIN32)
      add_test(NAME gui_smoke
        COMMAND hebocr-gui
          --screenshot ${CMAKE_CURRENT_BINARY_DIR}/gui_smoke.png
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/images/genesis_nikud.png)
      set_tests_properties(gui_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/gui_smoke_config"
        TIMEOUT 120)
    endif()
  else()
    message(STATUS "Qt 6 not found; not building hebocr-gui")
  endif()
endif()

install(TARGETS libhebocr ARCHIVE DESTINATION lib)
install(FILES src/hocr.h src/hocrpp.h DESTINATION include/hebocr)

# ---------------------------------------------------------------------------
# Tests: `ctest` runs the CLI on sample images
# ---------------------------------------------------------------------------
enable_testing()
if(HEBOCR_BUILD_CLI AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/images)
  file(GLOB HEBOCR_TEST_IMAGES ${CMAKE_CURRENT_SOURCE_DIR}/tests/images/*.png)
  foreach(img ${HEBOCR_TEST_IMAGES})
    get_filename_component(name ${img} NAME_WE)
    add_test(NAME ocr_${name}
      COMMAND ${CMAKE_COMMAND}
        -DHEBOCR=$<TARGET_FILE:hebocr>
        -DIMAGE=${img}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.txt
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/expected/${name}.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_ocr.cmake)
  endforeach()
endif()
