# lib/component CMakeLists.txt
# Component library for DELILA2

set(COMPONENT_SOURCES
    src/DigitizerSource.cpp
    src/FileWriter.cpp
    src/TimeSortMerger.cpp
    src/CLIOperator.cpp
)

set(COMPONENT_HEADERS
    include/DigitizerSource.hpp
    include/FileWriter.hpp
    include/TimeSortMerger.hpp
    include/CLIOperator.hpp
)

# Add sources to parent scope for unified library
set(COMPONENT_SOURCES ${COMPONENT_SOURCES} PARENT_SCOPE)
