cmake_minimum_required(VERSION 3.16)

project(hebocr VERSION 0.11.1 LANGUAGES C)

option(HEBOCR_WITH_TIFF "Read black-and-white TIFF files with libtiff" ON)
option(HEBOCR_WITH_FFTW "Use FFTW for image filtering" OFF)
option(HEBOCR_BUILD_CLI "Build the hebocr command-line tool (needs GLib)" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

string(TIMESTAMP HEBOCR_BUILD_DATE "%Y%m%d")
set(HEBOCR_BUILD_STR
    "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${HEBOCR_BUILD_DATE}")

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
