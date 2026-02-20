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

# Build Uboot Desktop EXE
add_executable(Uboot WIN32
    app/main.cpp
    app/win32/MainWindow.cpp
    app/win32/Layout.cpp
    app/win32/Tabs.cpp
    app/win32/EntriesListView.cpp
    app/win32/DetailPane.cpp
    app/resources/uboot.rc
    ${SOURCES}
)

# Ensure Unicode
target_compile_definitions(Uboot PRIVATE UNICODE _UNICODE STRICT WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)

# Link Windows libraries
target_link_libraries(Uboot PRIVATE taskschd comsupp shlwapi comctl32)
target_include_directories(Uboot PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
