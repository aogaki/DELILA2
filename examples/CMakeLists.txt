cmake_minimum_required(VERSION 3.15)
project(DELILA2Examples LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When building as part of main project, DELILA target is available
# When building standalone, find installed DELILA2
if(TARGET DELILA)
    # Building as part of main project - use local target
    message(STATUS "Using local DELILA target")
    set(DELILA_TARGET DELILA)
else()
    # Building standalone - find installed DELILA2
    find_package(DELILA2 REQUIRED)
    set(DELILA_TARGET DELILA2::DELILA)
endif()

# Find dependencies that DELILA2 uses
find_package(PkgConfig REQUIRED)

# Find ZMQ
pkg_check_modules(ZMQ libzmq)
if(ZMQ_FOUND AND NOT ZMQ_LIBRARIES MATCHES "^/")
    # pkg-config found it but only gave library name, find full path
    find_library(ZMQ_FULL_PATH
        NAMES ${ZMQ_LIBRARIES}
        PATHS ${ZMQ_LIBRARY_DIRS} /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(ZMQ_FULL_PATH)
        set(ZMQ_LIBRARIES ${ZMQ_FULL_PATH})
    endif()
endif()
if(NOT ZMQ_FOUND)
    # Try to find ZMQ manually
    find_path(ZMQ_INCLUDE_DIRS zmq.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    find_library(ZMQ_LIBRARIES
        NAMES zmq
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(ZMQ_INCLUDE_DIRS AND ZMQ_LIBRARIES)
        set(ZMQ_FOUND TRUE)
    endif()
endif()

# Verify all dependencies are found
if(NOT ZMQ_FOUND)
    message(FATAL_ERROR "ZeroMQ not found. Please install libzmq.")
endif()

# Optional: Find CAEN FELib if you want to build digitizer examples
find_library(CAEN_FELIB CAEN_FELib)

# ============================================================================
# BASIC EXAMPLES
# ============================================================================

# Example 1: Separated serialization demonstration (no CAEN dependency)
add_executable(separated_serialization separated_serialization.cpp)
target_link_libraries(separated_serialization
    PRIVATE
        ${DELILA_TARGET}
        ${ZMQ_LIBRARIES}
)
target_include_directories(separated_serialization
    PRIVATE
        ${ZMQ_INCLUDE_DIRS}
)

# Example 2: MinimalEventData usage (no CAEN dependency)
add_executable(minimal_event_data_example minimal_event_data_example.cpp)
target_link_libraries(minimal_event_data_example
    PRIVATE
        ${DELILA_TARGET}
        ${ZMQ_LIBRARIES}
)
target_include_directories(minimal_event_data_example
    PRIVATE
        ${ZMQ_INCLUDE_DIRS}
)

# ============================================================================
# CAEN FELib DEPENDENT EXAMPLES
# ============================================================================
if(CAEN_FELIB)
    message(STATUS "CAEN FELib found - building digitizer examples")

    # Basic library usage (requires Digitizer class)
    add_executable(basic_example basic_example.cpp)
    target_link_libraries(basic_example
        PRIVATE
            ${DELILA_TARGET}
            ${ZMQ_LIBRARIES}
    )
    target_include_directories(basic_example
        PRIVATE
            ${ZMQ_INCLUDE_DIRS}
    )

    # Simple digitizer test (requires CAEN hardware)
    add_executable(simple_digitizer_test simple_digitizer_test.cpp)
    target_link_libraries(simple_digitizer_test
        PRIVATE
            ${DELILA_TARGET}
            ${ZMQ_LIBRARIES}
    )
    target_include_directories(simple_digitizer_test
        PRIVATE
            ${ZMQ_INCLUDE_DIRS}
    )

    # Digitizer2 basic example (requires CAEN Digitizer 2 hardware)
    add_executable(digitizer2_basic_example digitizer2_basic_example.cpp)
    target_link_libraries(digitizer2_basic_example
        PRIVATE
            ${DELILA_TARGET}
            ${ZMQ_LIBRARIES}
    )
    target_include_directories(digitizer2_basic_example
        PRIVATE
            ${ZMQ_INCLUDE_DIRS}
    )
else()
    message(STATUS "CAEN FELib not found - skipping digitizer examples")
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

# Install examples source code
install(FILES
    CMakeLists.txt
    separated_serialization.cpp
    minimal_event_data_example.cpp
    DESTINATION share/delila2/examples
)

# Install CAEN-dependent examples source if available
if(CAEN_FELIB)
    install(FILES
        basic_example.cpp
        simple_digitizer_test.cpp
        digitizer2_basic_example.cpp
        DESTINATION share/delila2/examples
    )
endif()

# Install README if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    install(FILES README.md DESTINATION share/delila2/examples)
endif()

# Install built examples
install(TARGETS
    separated_serialization
    minimal_event_data_example
    DESTINATION bin
)

if(CAEN_FELIB)
    install(TARGETS
        basic_example
        simple_digitizer_test
        digitizer2_basic_example
        DESTINATION bin
    )
endif()

# ============================================================================
# BUILD INSTRUCTIONS
# ============================================================================

message(STATUS "")
message(STATUS "DELILA2 Examples Configuration:")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  DELILA2 found: ${DELILA2_FOUND}")
message(STATUS "  ZMQ: ${ZMQ_FOUND}")
if(ZMQ_FOUND)
    message(STATUS "    - Library: ${ZMQ_LIBRARIES}")
    message(STATUS "    - Include: ${ZMQ_INCLUDE_DIRS}")
endif()
message(STATUS "  CAEN FELib: ${CAEN_FELIB}")
message(STATUS "")
message(STATUS "To build examples:")
message(STATUS "  mkdir build")
message(STATUS "  cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "")