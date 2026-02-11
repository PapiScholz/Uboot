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

# Source files
set(SOURCES
    main.cpp
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
)

# Create executable
add_executable(uboot-collector ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(uboot-collector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAPIDJSON_INCLUDE_DIRS}
)

# Windows-specific libraries
target_link_libraries(uboot-collector PRIVATE
    advapi32    # Registry access
    shell32     # Shell functions
    ole32       # COM support
    oleaut32    # COM automation
    taskschd    # Task Scheduler
    wbemuuid    # WMI
    shlwapi     # Shell lightweight APIs
    pathcch     # Path handling (Windows 8+)
)

# Compiler options for MSVC
if(MSVC)
    target_compile_options(uboot-collector PRIVATE
        /W4                 # Warning level 4
        /WX-                # Do not treat warnings as errors (for now)
        /permissive-        # Conformance mode
        /Zc:__cplusplus     # Enable correct __cplusplus macro
        /EHsc               # Exception handling model
        /MP                 # Multi-processor compilation
        /utf-8              # UTF-8 source and execution charset
    )
    
    # Add debugging information for Debug builds
    target_compile_options(uboot-collector PRIVATE
        $<$<CONFIG:Debug>:/Zi /Od>
        $<$<CONFIG:Release>:/O2>
    )
    
    # Set subsystem to console
    set_target_properties(uboot-collector PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Set output name
set_target_properties(uboot-collector PROPERTIES
    OUTPUT_NAME "uboot-collector"
    DEBUG_POSTFIX "_debug"
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
