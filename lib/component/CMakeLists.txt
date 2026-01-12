# lib/component CMakeLists.txt
# Component library for DELILA2

set(COMPONENT_SOURCES
    src/DigitizerSource.cpp
    src/FileWriter.cpp
    src/SimpleMerger.cpp
    src/CLIOperator.cpp
    src/Emulator.cpp
)

set(COMPONENT_HEADERS
    include/DigitizerSource.hpp
    include/FileWriter.hpp
    include/SimpleMerger.hpp
    include/CLIOperator.hpp
    include/Emulator.hpp
)

# ROOT-dependent components (optional)
find_package(ROOT QUIET COMPONENTS Core Hist Graf HttpServer)

if(ROOT_FOUND)
    message(STATUS "ROOT found - building MonitorROOT")
    list(APPEND COMPONENT_SOURCES src/MonitorROOT.cpp)
    list(APPEND COMPONENT_HEADERS include/MonitorROOT.hpp)
    set(HAS_ROOT TRUE PARENT_SCOPE)
else()
    message(STATUS "ROOT not found - MonitorROOT will not be built")
    set(HAS_ROOT FALSE PARENT_SCOPE)
endif()

# Add sources to parent scope for unified library
set(COMPONENT_SOURCES ${COMPONENT_SOURCES} PARENT_SCOPE)
