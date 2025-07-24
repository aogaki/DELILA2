# Network library
cmake_minimum_required(VERSION 3.15)

# Find dependencies for network library (if any sources exist)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(LZ4 REQUIRED liblz4)
pkg_check_modules(XXHASH REQUIRED libxxhash)

# Collect source and header files (if they exist)
file(GLOB_RECURSE NET_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE NET_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Create network library
add_library(delila_net STATIC
    ${NET_SOURCES}
    ${NET_HEADERS}
)

target_include_directories(delila_net
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/delila/net>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ZMQ_INCLUDE_DIRS}
        ${LZ4_INCLUDE_DIRS}
        ${XXHASH_INCLUDE_DIRS}
)

target_link_libraries(delila_net
    PUBLIC delila_digitizer
    PRIVATE ${ZMQ_LIBRARIES} ${LZ4_LIBRARIES} ${XXHASH_LIBRARIES}
)

target_compile_features(delila_net PUBLIC cxx_std_17)
target_compile_definitions(delila_net PRIVATE ${ZMQ_CFLAGS_OTHER})

# Install headers (if they exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    install(DIRECTORY include/
        DESTINATION include/delila/net
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()