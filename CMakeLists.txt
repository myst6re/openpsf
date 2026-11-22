cmake_minimum_required(VERSION 3.25)

option(USE_STATIC_MSVC_RUNTIME "Use static MSVC runtime (/MT)" ON)

if(USE_STATIC_MSVC_RUNTIME AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(openpsf LANGUAGES C CXX VERSION 1.3.0)

option(BUILD_SHARED_LIBS "Is Dynamic" OFF)

set(PSFLIB_SHARED_OBJECT ON CACHE BOOL "" FORCE)
set(HE_SHARED_OBJECT ON CACHE BOOL "" FORCE)

# Pass -fPIC when openpsf is a shared library (Linux, macOS).
if(BUILD_SHARED_LIBS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory("psflib")
add_subdirectory("highly_experimental")

add_library(openpsf)
add_library(openpsf::openpsf ALIAS openpsf)

target_sources(openpsf
    PRIVATE
        src/circular_buffer.cpp
        src/openpsf_c.cpp
        src/openpsf.cpp
        src/PSXFilter.cpp
        $<TARGET_OBJECTS:psflib>
        $<TARGET_OBJECTS:highly_experimental>
)

target_include_directories(openpsf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:$<TARGET_PROPERTY:psflib,INTERFACE_INCLUDE_DIRECTORIES>>
        $<BUILD_INTERFACE:$<TARGET_PROPERTY:highly_experimental,INTERFACE_INCLUDE_DIRECTORIES>>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(openpsf INTERFACE $<INSTALL_INTERFACE:ZLIB::ZLIB>)

target_compile_definitions(openpsf
    PRIVATE
        OPENPSF_EXPORTS
)

# WIN32 must match the definition used to build highly_experimental
# (it selects the calling convention in emuconfig.h).
if(WIN32)
    target_compile_definitions(openpsf
        PRIVATE
            WIN32
            _WINDOWS
            _USRDLL
            UNICODE
            _UNICODE
    )
endif()

if(MSVC)
    target_compile_options(openpsf
        PRIVATE
            /W3
    )
endif()

# Installation

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS openpsf
    EXPORT openpsfTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT openpsfTargets
    FILE openpsfTargets.cmake
    NAMESPACE openpsf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openpsf
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/openpsfConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/openpsfConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/openpsfConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openpsf
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/openpsfConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/openpsfConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openpsf
)
