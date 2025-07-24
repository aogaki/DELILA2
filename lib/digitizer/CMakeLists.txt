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
    PRIVATE RHTTP gomp CAEN_FELib
)

target_compile_features(delila_digitizer PUBLIC cxx_std_17)
target_compile_options(delila_digitizer PRIVATE -pthread -O2 -fopenmp)

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