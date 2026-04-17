cmake_minimum_required(VERSION 3.15)
project(Uboot VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add reasonable warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
endif()

# Windows-specific configuration
if(NOT WIN32)
    message(FATAL_ERROR "Uboot is Windows-only")
endif()

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(UBOOT_ENABLE_SIGNING "Enable optional Authenticode signing targets" OFF)
option(UBOOT_SIGN_AFTER_BUILD "Sign uboot-core.exe immediately after build when signing is enabled" OFF)

set(UBOOT_SIGN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/windows/sign-file.ps1" CACHE FILEPATH "PowerShell signing helper")
set(UBOOT_SIGNTOOL "" CACHE FILEPATH "Path to signtool.exe. Empty means auto-detect in script.")
set(UBOOT_SIGN_CERT_SHA1 "" CACHE STRING "SHA1 thumbprint of the code-signing certificate in CurrentUser/My or LocalMachine/My")
set(UBOOT_SIGN_PFX_PATH "" CACHE FILEPATH "Optional path to a PFX certificate")
set(UBOOT_SIGN_PFX_PASSWORD "" CACHE STRING "Optional password for the PFX certificate")
set(UBOOT_SIGN_TIMESTAMP_URL "http://timestamp.digicert.com" CACHE STRING "RFC3161 or Authenticode timestamp URL")
set(UBOOT_SIGN_DESCRIPTION "Uboot core executable" CACHE STRING "File description used during signing")

# Find or download RapidJSON
find_package(RapidJSON QUIET)
if(NOT RapidJSON_FOUND)
    # If not found, use FetchContent to download it
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG v1.1.0
    )
    FetchContent_GetProperties(rapidjson)
    if(NOT rapidjson_POPULATED)
        FetchContent_Populate(rapidjson)
    endif()
    set(RAPIDJSON_INCLUDE_DIRS ${rapidjson_SOURCE_DIR}/include)
else()
    set(RAPIDJSON_INCLUDE_DIRS ${RapidJSON_INCLUDE_DIRS})
endif()

set(SOURCES
    core/tx/Transaction.cpp
    core/backup/BackupStore.cpp
    core/hardening/CriticalList.cpp
    core/hardening/SecurityLog.cpp
    core/hardening/DependencyAnalyzer.cpp
    core/ops/RegistryRenameValueOp.cpp
    core/ops/TaskToggleEnabledOp.cpp
    core/ops/ServiceStartTypeOp.cpp
    core/ops/OperationFactory.cpp
    core/runner/CollectorRunner.cpp
    core/util/CliArgs.cpp
    core/util/CommandResolver.cpp
    core/util/GlobalLock.cpp
    core/resolve/LnkResolver.cpp
    core/normalize/CommandNormalizer.cpp
    core/json/JsonWriter.cpp
    core/enumerators/RunRegistryCollector.cpp
    core/enumerators/ServicesCollector.cpp
    core/enumerators/StartupFolderCollector.cpp
    core/enumerators/ScheduledTasksCollector.cpp
    core/enumerators/WmiPersistenceCollector.cpp
    core/enumerators/WinlogonCollector.cpp
    core/enumerators/IfeoDebuggerCollector.cpp
)

# Build uboot-core CLI executable
add_executable(uboot-core
    cli/main.cpp
    ${SOURCES}
)

# Ensure Unicode
target_compile_definitions(uboot-core PRIVATE UNICODE _UNICODE STRICT WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)

# Link Windows libraries
target_link_libraries(uboot-core PRIVATE taskschd comsupp shlwapi comctl32)
target_include_directories(uboot-core PRIVATE ${RAPIDJSON_INCLUDE_DIRS})

if(UBOOT_ENABLE_SIGNING)
    if(NOT EXISTS "${UBOOT_SIGN_SCRIPT}")
        message(FATAL_ERROR "UBOOT_ENABLE_SIGNING is ON but signing script was not found: ${UBOOT_SIGN_SCRIPT}")
    endif()

    add_custom_target(sign-uboot-core
        COMMAND powershell -ExecutionPolicy Bypass -File "${UBOOT_SIGN_SCRIPT}"
            -FilePath "$<TARGET_FILE:uboot-core>"
            -SignToolPath "${UBOOT_SIGNTOOL}"
            -CertSha1 "${UBOOT_SIGN_CERT_SHA1}"
            -PfxPath "${UBOOT_SIGN_PFX_PATH}"
            -PfxPassword "${UBOOT_SIGN_PFX_PASSWORD}"
            -TimestampUrl "${UBOOT_SIGN_TIMESTAMP_URL}"
            -Description "${UBOOT_SIGN_DESCRIPTION}"
        DEPENDS uboot-core
        COMMENT "Signing uboot-core.exe with Authenticode"
        VERBATIM
    )

    if(UBOOT_SIGN_AFTER_BUILD)
        add_custom_command(TARGET uboot-core POST_BUILD
            COMMAND powershell -ExecutionPolicy Bypass -File "${UBOOT_SIGN_SCRIPT}"
                -FilePath "$<TARGET_FILE:uboot-core>"
                -SignToolPath "${UBOOT_SIGNTOOL}"
                -CertSha1 "${UBOOT_SIGN_CERT_SHA1}"
                -PfxPath "${UBOOT_SIGN_PFX_PATH}"
                -PfxPassword "${UBOOT_SIGN_PFX_PASSWORD}"
                -TimestampUrl "${UBOOT_SIGN_TIMESTAMP_URL}"
                -Description "${UBOOT_SIGN_DESCRIPTION}"
            COMMENT "Signing uboot-core.exe after build"
            VERBATIM
        )
    endif()
endif()
