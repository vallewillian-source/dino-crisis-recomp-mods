cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "This project expects the psxrecomp framework checkout at ${PSXRECOMP_ROOT}.\n"
        "Run: git submodule update --init --recursive\n"
        "If psxrecomp/ is not a submodule, set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(Dino_Crisis_Recompiled LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# Scaffold prompts / flags fill these. Without recomp-ui, PSX_RECOMP_UI is OFF.
# Setup-host CI requires WIZARD_CMAKE_BLOCK / ENABLE_SETUP_WIZARD (FORCE_SETUP_HOST
# alone does not open first-run / Generate & rebuild).
set(PSX_RECOMP_UI ON CACHE BOOL "Shared launcher" FORCE)
set(PSX_DEBUG_TOOLS OFF CACHE BOOL "Production build")
set(PSX_STATIC_RUNTIME ON CACHE BOOL "Portable runtime" FORCE)
set(PSX_ENABLE_VULKAN ON CACHE BOOL "Vulkan renderer" FORCE)
set(PSX_NETPLAY OFF CACHE BOOL "Accepted offline release" FORCE)
set(PSX_SETUP_WIZARD ON CACHE BOOL "First-run setup" FORCE)
set(PSXRECOMP_BIOS_STEMS "SCPH5552" CACHE STRING "Accepted BIOS" FORCE)
set(PSXRECOMP_BIOS_STEM "SCPH5552" CACHE STRING "Accepted BIOS" FORCE)
set(PSXRECOMP_BIOS_PROFILE "${PSXRECOMP_ROOT}/bios/SCPH5552.toml" CACHE FILEPATH "Accepted BIOS" FORCE)
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# --- Fase 2: substituições matched (docs/MATCHED.md) -----------------------
# Os segmentos listados em src_game/replacements.txt são REMOVIDOS dos shards
# gerados (cópia filtrada em build/gen-src/) e re-implementados em C legível
# em src_game/. Roda no configure; após `generate`, reconfigure (regra já
# estabelecida do projeto).
set(PSX_GEN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(PSX_GEN_SRC  "${CMAKE_CURRENT_BINARY_DIR}/gen-src")
set(PSX_TEST_SRC "${CMAKE_CURRENT_BINARY_DIR}/matched-test")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB PSX_GEN_SHARDS "${PSX_GEN_DIR}/SLUS_009.22_full_*.c")
if(PSX_GEN_SHARDS)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PSX_GEN_SRC}" "${PSX_TEST_SRC}"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/filter_generated.py"
                --mode filter --gen "${PSX_GEN_DIR}" --out "${PSX_GEN_SRC}"
                --replacements "${CMAKE_CURRENT_SOURCE_DIR}/src_game/replacements.txt"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/filter_generated.py"
                --mode extract --gen "${PSX_GEN_DIR}" --out "${PSX_TEST_SRC}"
                --replacements "${CMAKE_CURRENT_SOURCE_DIR}/src_game/replacements.txt"
        RESULT_VARIABLE _psx_filter_res)
    if(NOT _psx_filter_res EQUAL 0)
        message(FATAL_ERROR "filter_generated.py falhou (ver replacements.txt x generated/)")
    endif()
endif()

psxrecomp_add_game_runtime(psx-runtime


    ENABLE_SETUP_WIZARD
    EXE_NAME "Dino_Crisis_Recompiled"
    WINDOW_TITLE "Dino Crisis Recompiled"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 1


    GEN_MARKER "${PSX_GEN_SRC}/SLUS_009.22_dispatch.c"
    GEN_FULL_GLOB "${PSX_GEN_SRC}/SLUS_009.22_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
)

# Substituições matched linkadas no runtime (fornecem os símbolos cujos
# corpos foram removidos dos shards filtrados).
target_sources(psx-runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src_game/event_flags.c")
target_include_directories(psx-runtime PRIVATE "${PSX_GEN_SRC}")

# ---- psx-matched-test: equivalência original vs substituição -------------
if(PSX_GEN_SHARDS AND EXISTS "${PSX_TEST_SRC}/orig_extracted.c")
    add_executable(psx-matched-test
        "${PSX_TEST_SRC}/orig_extracted.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src_game/event_flags.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src_game/matched_test.c")
    target_include_directories(psx-matched-test PRIVATE
        "${PSX_GEN_DIR}" "${PSX_GEN_DIR}/.." )
    target_include_directories(psx-matched-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp/runtime/include")
    target_compile_definitions(psx-matched-test PRIVATE PSX_NO_DEBUG_TOOLS=1)
    set_source_files_properties("${PSX_TEST_SRC}/orig_extracted.c" PROPERTIES
        COMPILE_DEFINITIONS "func_8004A87C=orig_8004A87C;func_8004A7E8=orig_8004A7E8;func_8004A830=orig_8004A830")
    add_custom_command(TARGET psx-matched-test POST_BUILD
        COMMAND $<TARGET_FILE:psx-matched-test>
        COMMENT "psx-matched-test: verificando equivalência original vs substituição")
endif()

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# --- Fase 3: catálogo de mods do jogo --------------------------------------
# O framework limpa mods/packages e recopia o catálogo builtin a cada build
# (POST_BUILD declarado antes dos nossos); o nosso POST_BUILD roda depois e
# recoloca os pacotes game-owned (versionados em repo/mods/packages).
# state.toml (mods/) é user state e sobrevive ao rebuild.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mods/packages")
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/mods/packages"
            "$<TARGET_FILE_DIR:psx-runtime>/mods/packages"
        COMMENT "Staging game-owned mod catalog"
        VERBATIM)
endif()

# --- Progressive symbol map (STEPS.md Fase 0.5) ---------------------------
# symbols.toml (hand-curated + psxrecomp-analyze append-only) regenerates
# psx_symbols.h (PSX_FN_* / func_<pc> aliases) on every build. The header is
# includable from any psx-runtime TU; decompiled C (Fase 2) calls game code
# through PSX_FN_<name>.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/psx_symbols.h"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp/tools/new_project_layout/sync_symbols.py"
            --symbols "${CMAKE_CURRENT_SOURCE_DIR}/symbols.toml"
            --header "${CMAKE_CURRENT_SOURCE_DIR}/psx_symbols.h"
            --game "Dino Crisis"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/symbols.toml"
    COMMENT "sync_symbols.py: symbols.toml -> psx_symbols.h"
    VERBATIM)
add_custom_target(psx-symbols ALL
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/psx_symbols.h")
add_dependencies(psx-runtime psx-symbols)
target_include_directories(psx-runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
