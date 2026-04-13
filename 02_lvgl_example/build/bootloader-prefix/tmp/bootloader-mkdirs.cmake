# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/blake/esp/v5.5.3/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "C:/Users/blake/esp/v5.5.3/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader"
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix"
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/tmp"
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src"
  "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/blake/Downloads/ESP32-S3-Touch-LCD-3.5-Demo/ESP-IDF/02_lvgl_example/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
