# Digitizer library
cmake_minimum_required(VERSION 3.15)

# Find required dependencies for digitizer
find_package(ROOT REQUIRED COMPONENTS RIO Net)
include(${ROOT_USE_FILE})

# Collect source and header files
file(GLOB_RECURSE DIGITIZER_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE DIGITIZER_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Create digitizer library
add_library(delila_digitizer STATIC 
    ${DIGITIZER_SOURCES}
    ${DIGITIZER_HEADERS}
)

target_include_directories(delila_digitizer
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/delila/digitizer>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ROOT_INCLUDE_DIRS}
)

target_link_libraries(delila_digitizer
    PUBLIC ${ROOT_LIBRARIES}
)

# Try to link RHTTP if available (part of ROOT)
find_library(RHTTP_LIB RHTTP PATHS ${ROOT_LIBRARY_DIR})
if(RHTTP_LIB)
    target_link_libraries(delila_digitizer PRIVATE ${RHTTP_LIB})
    message(STATUS "Found RHTTP: ${RHTTP_LIB}")
else()
    message(STATUS "RHTTP not found - building without RHTTP support")
endif()

# Try to link CAEN library if available
find_library(CAEN_FELIB CAEN_FELib)
if(CAEN_FELIB)
    target_link_libraries(delila_digitizer PRIVATE CAEN_FELib)
    message(STATUS "Found CAEN_FELib: ${CAEN_FELIB}")
else()
    message(STATUS "CAEN_FELib not found - building without CAEN support")
endif()

target_compile_features(delila_digitizer PUBLIC cxx_std_17)

# Add compile options based on compiler and platform
target_compile_options(delila_digitizer PRIVATE -pthread -O2)

# Try to find and use OpenMP if available
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(delila_digitizer PRIVATE OpenMP::OpenMP_CXX)
else()
    message(STATUS "OpenMP not found - building without OpenMP support")
endif()

# Copy configuration files to build directory
file(GLOB DIGITIZER_CONF_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json" 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.conf"
)
file(COPY ${DIGITIZER_CONF_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Install headers
install(DIRECTORY include/
    DESTINATION include/delila/digitizer
    FILES_MATCHING PATTERN "*.hpp"
)