#
# leptonica
#

# Modified by Luc Michaslki - 2017

###############################################################################
#
# cmake settings
#
###############################################################################

cmake_minimum_required(VERSION 2.8.11)

#-----------------------------------------------------------------------------
# Hunter - bootsrap gate
#-----------------------------------------------------------------------------

  include("cmake/HunterGate.cmake")
  HunterGate(
      URL "https://github.com/ruslo/hunter/archive/v0.18.15.tar.gz"
      SHA1 "57991b41173c0e3c7501c7929cad8323277fab4c"
  )

# In-source builds are disabled.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR
        "CMake generation is not possible within the source directory!"
        "\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
        "\n "
        "\n rm CMakeCache.txt"
        "\n mkdir build"
        "\n cd build"
        "\n cmake .."
    )
endif()

# build type
option(BUILD_SHARED_LIBS  "Build shared libraries"                ON) 

# dependencies
option(WITH_JPEG          "Build with JPEG image format support"  ON)
option(WITH_PNG           "Build with PNG image format support"   ON)
option(WITH_TIFF          "Build with TIFF image format support"  ON)
option(WITH_ZLIB          "Build with ZLIB compression support"   ON)
option(WITH_GIF           "Build with GIF image format support"   OFF)

# build components
option(BUILD_PROG         "Build BUILD_PROG"                      OFF)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake")

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

###############################################################################
#
# project settings
#
###############################################################################

project(leptonica C CXX)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 74)
set(VERSION_PATCH 1)
set(VERSION_PLAIN ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/.cppan)

    # ---[ GIF
    if(WITH_GIF)
      # ref. 
      #hunter_add_package(GIF)
      #if(HUNTER_ENABLED)
      #  find_package(GIF CONFIG REQUIRED)
      #  list(APPEND ${PROJECT_NAME}_DEPENDENCY_INTERFACES GIF::gif)
      #  set(GIF_LIBRARY GIF::gif)
      #else()    
      find_package(GIF QUIET)
      #endif(HUNTER_ENABLED)
    endif(WITH_GIF)

    # ---[ Jpeg
    if(WITH_JPEG)
      # ref. https://github.com/ruslo/hunter/blob/master/examples/Jpeg/CMakeLists.txt
      hunter_add_package(Jpeg)
      find_package(JPEG CONFIG REQUIRED)
      if(HUNTER_ENABLED)
        find_package(JPEG CONFIG REQUIRED)
        list(APPEND ${PROJECT_NAME}_DEPENDENCY_INTERFACES JPEG::jpeg)
        set(JPEG_LIBRARY JPEG::jpeg)
      else()
        find_package(JPEG)
      endif(HUNTER_ENABLED)
    endif(WITH_JPEG)

    # ---[ PNG
    if(WITH_PNG)
      # ref. https://github.com/ruslo/hunter/blob/master/examples/PNG/CMakeLists.txt
      hunter_add_package(PNG)
      if(HUNTER_ENABLED)
        find_package(PNG CONFIG REQUIRED)
        list(APPEND ${PROJECT_NAME}_DEPENDENCY_INTERFACES PNG::png)
        set(PNG_LIBRARY PNG::png)
      else()
        find_package(PNG)
      endif(HUNTER_ENABLED)
    endif(WITH_PNG)

    # ---[ TIFF
    # ref. https://github.com/ruslo/hunter/blob/master/examples/TIFF/CMakeLists.txt#L21
    if(WITH_TIFF)
      hunter_add_package(TIFF)
      if(HUNTER_ENABLED)
        find_package(TIFF CONFIG REQUIRED)
        list(APPEND ${PROJECT_NAME}_DEPENDENCY_INTERFACES TIFF::libtiff)
        set(TIFF_LIBRARY TIFF::libtiff)
      else()
        find_package(TIFF)
      endif(HUNTER_ENABLED)
    endif(WITH_TIFF)

    # ---[ ZLIB
    if(WITH_ZLIB)
      # ref. https://github.com/ruslo/hunter/tree/master/examples/ZLIB
      hunter_add_package(ZLIB)
      if(HUNTER_ENABLED)
        find_package(ZLIB CONFIG REQUIRED)
        list(APPEND ${PROJECT_NAME}_DEPENDENCY_INTERFACES ZLIB::zlib)
        set(ZLIB_LIBRARY ZLIB::zlib)
      else()
        find_package(ZLIB)
      endif(HUNTER_ENABLED)
    endif(WITH_ZLIB)

    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(WEBP libwebp)
      pkg_check_modules(JP2K libopenjp2)
    endif()

else()

    add_subdirectory(.cppan)

    add_definitions(
        -DHAVE_LIBGIF=1
        -DHAVE_LIBJPEG=1
        -DHAVE_LIBPNG=1
        -DHAVE_LIBTIFF=1
        -DHAVE_LIBWEBP=1
        -DHAVE_LIBZ=1

        -DHAVE_LIBJP2K=1
        -DLIBJP2K_HEADER="openjpeg.h"
    )
endif()

###############################################################################
#
# compiler and linker
#
###############################################################################
# switch LIBRARY_TYPE accordingly to BUILD_SHARED_LIBS arguments/vars if passed
set(LIBRARY_TYPE SHARED)
if(STATIC OR NOT BUILD_SHARED_LIBS)
    set(LIBRARY_TYPE)
endif(STATIC OR NOT BUILD_SHARED_LIBS)

if (WIN32)
    if (MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    endif()
endif()

###############################################################################
#
# configure
#
###############################################################################

set(AUTOCONFIG_SRC ${CMAKE_BINARY_DIR}/config_auto.h.in)
set(AUTOCONFIG ${CMAKE_BINARY_DIR}/src/config_auto.h)

include(Configure)

configure_file(${AUTOCONFIG_SRC} ${AUTOCONFIG} @ONLY)

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/src")

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/LeptonicaConfig-version.cmake.in
    ${CMAKE_BINARY_DIR}/leptonicaConfig-version.cmake @ONLY)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/LeptonicaConfig.cmake.in
    ${CMAKE_BINARY_DIR}/leptonicaConfig.cmake @ONLY)

set(version_config "${CMAKE_BINARY_DIR}/leptonicaConfig-version.cmake")
set(project_config "${CMAKE_BINARY_DIR}/leptonicaConfig.cmake")  

###############################################################################
#
# build
#
###############################################################################

add_definitions(-DHAVE_CONFIG_H)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

if (BUILD_PROG)
  add_subdirectory(prog)
endif()

add_subdirectory(src)

###############################################################################
#
# install
#
###############################################################################

set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(namespace "${PROJECT_NAME}::")
set(targets_export_name "${PROJECT_NAME}Targets")

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)

###############################################################################

###############################################################################
