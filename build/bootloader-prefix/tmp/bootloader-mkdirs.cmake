# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/hamza/esp/v5.2.1/esp-idf/components/bootloader/subproject"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/tmp"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/src/bootloader-stamp"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/src"
  "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/hamza/ESP32_projects/NMEA_PARSER/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
