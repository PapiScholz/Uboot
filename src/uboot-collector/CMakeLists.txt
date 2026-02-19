cmake_minimum_required(VERSION 3.15)
project(uboot-collector VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific configuration
if(NOT WIN32)
    message(FATAL_ERROR "uboot-collector is Windows-only")
endif()

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
    FetchContent_MakeAvailable(rapidjson)
    set(RAPIDJSON_INCLUDE_DIRS ${rapidjson_SOURCE_DIR}/include)
else()
    set(RAPIDJSON_INCLUDE_DIRS ${RapidJSON_INCLUDE_DIRS})
endif()

# Source files (excluding main.cpp for reusability)
set(SOURCES
    util/CliArgs.cpp
    util/CommandResolver.cpp
    runner/CollectorRunner.cpp
    json/JsonWriter.cpp
    normalize/CommandNormalizer.cpp
    resolve/LnkResolver.cpp
    collectors/RunRegistryCollector.cpp
    collectors/ServicesCollector.cpp
    collectors/StartupFolderCollector.cpp
    collectors/ScheduledTasksCollector.cpp
    collectors/WmiPersistenceCollector.cpp
    collectors/WinlogonCollector.cpp
    collectors/IfeoDebuggerCollector.cpp
    evidence/file_probe.cpp
    evidence/hashing.cpp
    evidence/version_info.cpp
    evidence/authenticode.cpp
    evidence/misconfig_checks.cpp
    evidence/entry_evidence.cpp
    scoring/RulesLoader.cpp
    scoring/ScoringEngine.cpp
)

# Header files (for IDE visibility)
set(HEADERS
    model/Entry.h
    model/CollectorError.h
    collectors/ICollector.h
    collectors/RunRegistryCollector.h
    collectors/ServicesCollector.h
    collectors/StartupFolderCollector.h
    collectors/ScheduledTasksCollector.h
    collectors/WmiPersistenceCollector.h
    collectors/WinlogonCollector.h
    collectors/IfeoDebuggerCollector.h
    util/CliArgs.h
    util/CommandResolver.h
    runner/CollectorRunner.h
    json/JsonWriter.h
    normalize/CommandNormalizer.h
    resolve/LnkResolver.h
    evidence/file_probe.h
    evidence/hashing.h
    evidence/version_info.h
    evidence/authenticode.h
    evidence/misconfig_checks.h
    evidence/entry_evidence.h
    scoring/Types.h
    scoring/RulesLoader.h
    scoring/ScoringEngine.h
    scoring/ThresholdClassifier.h
    scoring/IScoringStrategy.h
    scoring/ISignalEvaluator.h
    scoring/SignalRegistry.h
    scoring/FutureStubs.h
    scoring/strategies/WeightedSumStrategy.h
    scoring/evaluators/UnsignedBinaryEvaluator.h
    scoring/evaluators/InvalidSignatureEvaluator.h
    scoring/evaluators/MissingTargetEvaluator.h
    scoring/evaluators/UserWritableEvaluator.h
    scoring/evaluators/LolbinEvaluator.h
    scoring/evaluators/ObfuscatedCommandEvaluator.h
    scoring/evaluators/RelativePathEvaluator.h
    scoring/evaluators/System32LegitEvaluator.h
)

# ... (Headers kept as is) ...

# Create main executable
add_executable(uboot-collector main.cpp ${SOURCES} ${HEADERS})

# ... (Include directories and Link libraries for uboot-collector kept as is) ...

# Test executable
add_executable(test_scoring tests/test_scoring.cpp ${SOURCES} ${HEADERS})

target_include_directories(test_scoring PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAPIDJSON_INCLUDE_DIRS}
)

# Phase 3 Verification Test
add_executable(test_phase3 tests/test_phase3.cpp ${SOURCES} ${HEADERS})

target_include_directories(test_phase3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAPIDJSON_INCLUDE_DIRS}
)

target_link_libraries(test_phase3 PRIVATE
    advapi32
    shell32
    bcrypt
    wintrust
    crypt32
    version
    ole32
    oleaut32
    taskschd
    wbemuuid
    shlwapi
    pathcch
)

target_link_libraries(test_scoring PRIVATE
    advapi32
    shell32
    bcrypt
    wintrust
    crypt32
    version
    ole32
    oleaut32
    taskschd
    wbemuuid
    shlwapi
    pathcch
)

# Installation rules (optional)
install(TARGETS uboot-collector
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════")
message(STATUS "  uboot-collector Configuration Summary")
message(STATUS "═══════════════════════════════════════════════")
message(STATUS "  CMake Version:     ${CMAKE_VERSION}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  RapidJSON:         ${RAPIDJSON_INCLUDE_DIRS}")
message(STATUS "  Output Directory:  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "═══════════════════════════════════════════════")
message(STATUS "")
