cmake_minimum_required(VERSION 3.20)
project(Fireteam LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Fireteam currently builds with Visual Studio/MSVC on Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "LithTech Jupiter requires a 32-bit build. Configure with -A Win32.")
endif()

set(SEAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.local/imports/sealhunter")
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.local/imports/engine")

if(NOT EXISTS "${SEAL_ROOT}/cshell/src/ltclientshell.cpp")
    message(FATAL_ERROR "Local SealHunter source is missing. Run setup-local.cmd first.")
endif()
if(NOT EXISTS "${ENGINE_ROOT}/sdk/inc/iltclient.h")
    message(FATAL_ERROR "Local Jupiter SDK source is missing. Run setup-local.cmd first.")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(FIRETEAM_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${FIRETEAM_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${FIRETEAM_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()

add_compile_definitions(WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE NOMINMAX)
add_compile_options(/MP /W3)

set(SDK_DIR "${ENGINE_ROOT}/sdk/inc")
set(CLIENTFX_SHARED_DIR "${ENGINE_ROOT}/clientfx/Shared")
set(GAME_SHARED_DIR "${SEAL_ROOT}/shared/src")

file(GLOB CSHELL_GAME_SOURCES CONFIGURE_DEPENDS "${SEAL_ROOT}/cshell/src/*.cpp" "${SEAL_ROOT}/cshell/src/*.h")
set(CSHELL_ENGINE_SOURCES
    "${SDK_DIR}/iltbaseclass.cpp"
    "${SDK_DIR}/ltmodule.cpp"
    "${CLIENTFX_SHARED_DIR}/ClientFXDB.cpp"
    "${CLIENTFX_SHARED_DIR}/ClientFXMgr.cpp"
    "${CLIENTFX_SHARED_DIR}/CommonUtilities.cpp"
    "${CLIENTFX_SHARED_DIR}/ParsedMsg.cpp"
    "${CLIENTFX_SHARED_DIR}/WinUtil.cpp"
)
add_library(cshell SHARED ${CSHELL_GAME_SOURCES} ${CSHELL_ENGINE_SOURCES})
target_compile_definitions(cshell PRIVATE _CLIENTBUILD)
target_include_directories(cshell PRIVATE "${SEAL_ROOT}/cshell/src" "${GAME_SHARED_DIR}" "${SDK_DIR}" "${CLIENTFX_SHARED_DIR}")
target_link_libraries(cshell PRIVATE odbc32 odbccp32)
set_target_properties(cshell PROPERTIES OUTPUT_NAME "cshell" PREFIX "")

file(GLOB SSHELL_GAME_SOURCES CONFIGURE_DEPENDS "${SEAL_ROOT}/sshell/src/*.cpp" "${SEAL_ROOT}/sshell/src/*.h")
set(SSHELL_ENGINE_SOURCES
    "${SDK_DIR}/iltbaseclass.cpp"
    "${SDK_DIR}/ltengineobjects.cpp"
    "${SDK_DIR}/ltmodule.cpp"
    "${SDK_DIR}/ltquatbase.cpp"
    "${CLIENTFX_SHARED_DIR}/ParsedMsg.cpp"
)
add_library(object_module SHARED ${SSHELL_GAME_SOURCES} ${SSHELL_ENGINE_SOURCES})
target_include_directories(object_module PRIVATE "${SEAL_ROOT}/sshell/src" "${GAME_SHARED_DIR}" "${SDK_DIR}" "${CLIENTFX_SHARED_DIR}")
target_link_libraries(object_module PRIVATE odbc32 odbccp32)
set_target_properties(object_module PROPERTIES OUTPUT_NAME "object" PREFIX "" SUFFIX ".lto")

# The original resource scripts contain no game resources but include old MFC headers.
# Minimal DLL stubs avoid requiring the MFC workload for the first build gate.
add_library(cres SHARED "${SEAL_ROOT}/cres/src/main.cpp")
set_target_properties(cres PROPERTIES OUTPUT_NAME "cres" PREFIX "")
add_library(sres SHARED "${SEAL_ROOT}/sres/src/main.cpp")
set_target_properties(sres PROPERTIES OUTPUT_NAME "sres" PREFIX "")


# Build Fireteam's ClientFX plugin from the original Jupiter 2006 source.
# This keeps the runtime ABI matched to our SDK/source instead of dropping in
# Combat Arms' prebuilt ClientFx.fxd.
set(CLIENTFX_ROOT "\${ENGINE_ROOT}/clientfx")
set(CLIENTFX_PLUGIN_SOURCES
    "\${CLIENTFX_ROOT}/BaseFx.cpp"
    "\${CLIENTFX_ROOT}/bouncychunkfx.cpp"
    "\${CLIENTFX_ROOT}/camjitterfx.cpp"
    "\${CLIENTFX_ROOT}/camwobblefx.cpp"
    "\${CLIENTFX_ROOT}/clientfx.cpp"
    "\${CLIENTFX_ROOT}/CreateFX.cpp"
    "\${CLIENTFX_ROOT}/cycletimer.cpp"
    "\${CLIENTFX_ROOT}/dynalightfx.cpp"
    "\${CLIENTFX_ROOT}/fallingstufffx.cpp"
    "\${CLIENTFX_ROOT}/FlareSpriteFX.cpp"
    "\${CLIENTFX_ROOT}/lightningfx.cpp"
    "\${CLIENTFX_ROOT}/LTBbouncychunkfx.cpp"
    "\${CLIENTFX_ROOT}/ltbmodelfx.cpp"
    "\${CLIENTFX_ROOT}/nullfx.cpp"
    "\${CLIENTFX_ROOT}/ParticleSystemFX.cpp"
    "\${CLIENTFX_ROOT}/playrandomsoundfx.cpp"
    "\${CLIENTFX_ROOT}/playsoundfx.cpp"
    "\${CLIENTFX_ROOT}/polyfanfx.cpp"
    "\${CLIENTFX_ROOT}/polytubefx.cpp"
    "\${CLIENTFX_ROOT}/spritefx.cpp"
    "\${CLIENTFX_ROOT}/spritesystemfx.cpp"
    "\${CLIENTFX_ROOT}/stdafx.cpp"
    "\${CLIENTFX_ROOT}/Shared/DebugNew.cpp"
    "\${SDK_DIR}/ltmodule.cpp"
    "\${SDK_DIR}/ltquatbase.cpp"
)

if(EXISTS "\${CLIENTFX_ROOT}/clientfx.cpp")
    add_library(clientfx_fxd SHARED \${CLIENTFX_PLUGIN_SOURCES})
    target_compile_definitions(clientfx_fxd PRIVATE NOPS2 MEMTRACK_CLIENTFX)
    target_include_directories(clientfx_fxd PRIVATE
        "\${CLIENTFX_ROOT}"
        "\${CLIENTFX_ROOT}/Shared"
        "\${SDK_DIR}"
    )
    target_link_libraries(clientfx_fxd PRIVATE odbc32 odbccp32)
    set_target_properties(clientfx_fxd PROPERTIES
        OUTPUT_NAME "ClientFx"
        PREFIX ""
        SUFFIX ".fxd"
    )
else()
    message(FATAL_ERROR "Original ClientFX plugin source is missing from EngineMissing.zip.")
endif()
