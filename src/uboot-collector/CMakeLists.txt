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
    core/hardening/PolicyInspector.cpp
    core/hardening/SecurityLog.cpp
    core/hardening/DependencyAnalyzer.cpp
    core/ops/RegistryRenameValueOp.cpp
    core/ops/TaskToggleEnabledOp.cpp
    core/ops/ServiceStartTypeOp.cpp
    core/ops/OperationFactory.cpp
    runner/CollectorRunner.cpp
    util/CliArgs.cpp
    util/CommandResolver.cpp
    util/GlobalLock.cpp
    resolve/LnkResolver.cpp
    normalize/CommandNormalizer.cpp
    json/JsonWriter.cpp
    collectors/RunRegistryCollector.cpp
    collectors/ServicesCollector.cpp
    collectors/StartupFolderCollector.cpp
    collectors/ScheduledTasksCollector.cpp
    collectors/WmiPersistenceCollector.cpp
    collectors/WinlogonCollector.cpp
    collectors/IfeoDebuggerCollector.cpp
)

# Header files (for IDE visibility)
set(HEADERS
)

# Source files (excluding main.cpp for reusability)
# SOURCES is already set

add_executable(uboot-collector main.cpp ${SOURCES})
target_link_libraries(uboot-collector PRIVATE taskschd comsupp shlwapi)
target_include_directories(uboot-collector PRIVATE ${RAPIDJSON_INCLUDE_DIRS})

# Test Ops
add_executable(test_ops tests/test_ops.cpp ${SOURCES})
target_link_libraries(test_ops PRIVATE taskschd comsupp shlwapi)
target_include_directories(test_ops PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
