cmake_minimum_required(VERSION 3.16)
project(vlc_openpsf C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ZLIB_USE_STATIC_LIBS ON)

find_package(openpsf REQUIRED)

# Création de la cible du plugin
add_library(openpsf_plugin MODULE src/vlc_openpsf_plugin.c)

# Convention de nommage obligatoire pour VLC sur tous les OS (lib<nom>_plugin)
set_target_properties(openpsf_plugin PROPERTIES
    PREFIX "lib"
    C_VISIBILITY_PRESET hidden
)

target_compile_definitions(openpsf_plugin PRIVATE
    __PLUGIN__
    MODULE_STRING="openpsf"
)

target_include_directories(openpsf_plugin
    PRIVATE
        $<BUILD_INTERFACE:$<TARGET_PROPERTY:openpsf::openpsf,INTERFACE_INCLUDE_DIRECTORIES>>
)

# 1. Tentative de détection via pkg-config (Linux, macOS, ou MSYS2 configuré)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(VLC_PLUGIN QUIET IMPORTED_TARGET vlc-plugin)
endif()

# 2. Choix de la méthode de liaison
if(TARGET PkgConfig::VLC_PLUGIN)
    message(STATUS "VLC SDK trouvé via pkg-config")
    target_link_libraries(openpsf_plugin PRIVATE PkgConfig::VLC_PLUGIN openpsf::openpsf z)
else()
    message(STATUS "pkg-config ou vlc-plugin introuvable, utilisation de VLC_SDK_DIR")
    
    # Chemin par défaut sous Windows (surchargeable via -DVLC_SDK_DIR=...)
    set(VLC_SDK_DIR "C:/Program Files/VideoLAN/VLC/sdk" CACHE PATH "Chemin vers le dossier sdk de VLC")

    if(NOT EXISTS "${VLC_SDK_DIR}/include/vlc/plugins/vlc_common.h")
        message(FATAL_ERROR "SDK VLC introuvable dans '${VLC_SDK_DIR}'. Spécifiez le chemin avec -DVLC_SDK_DIR=...")
    endif()

    if(WIN32)
        target_compile_definitions(openpsf_plugin PRIVATE _WIN32_WINNT=0x0601)
    endif()

    target_include_directories(openpsf_plugin PRIVATE
        "${VLC_SDK_DIR}/include"
        "${VLC_SDK_DIR}/include/vlc/plugins"
    )

    target_link_directories(openpsf_plugin PRIVATE
        "${VLC_SDK_DIR}/lib"
    )

    target_link_libraries(openpsf_plugin PRIVATE
        vlccore
        vlc
        openpsf::openpsf
        z
    )
endif()

if(MINGW)
    target_link_options(openpsf_plugin PRIVATE -static
        -static-libgcc
        -static-libstdc++)
endif()
