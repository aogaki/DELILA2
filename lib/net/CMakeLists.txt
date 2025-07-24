# Network library
cmake_minimum_required(VERSION 3.15)

# Find optional dependencies for network library
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ libzmq)
pkg_check_modules(LZ4 liblz4)
pkg_check_modules(XXHASH libxxhash)

# Collect source and header files (if they exist)
file(GLOB_RECURSE NET_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE NET_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Create network library (add a dummy source if no sources exist)
if(NOT NET_SOURCES)
    # Create a dummy source file for empty library
    set(DUMMY_NET_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/dummy_net.cpp")
    file(WRITE ${DUMMY_NET_SOURCE} "// Dummy source file for empty network library\n")
    set(NET_SOURCES ${DUMMY_NET_SOURCE})
endif()

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
)

# Add optional include directories
if(ZMQ_FOUND)
    target_include_directories(delila_net PRIVATE ${ZMQ_INCLUDE_DIRS})
endif()
if(LZ4_FOUND)
    target_include_directories(delila_net PRIVATE ${LZ4_INCLUDE_DIRS})
endif()
if(XXHASH_FOUND)
    target_include_directories(delila_net PRIVATE ${XXHASH_INCLUDE_DIRS})
endif()

target_link_libraries(delila_net PUBLIC delila_digitizer)

# Link optional dependencies
if(ZMQ_FOUND)
    target_link_libraries(delila_net PRIVATE ${ZMQ_LIBRARIES})
endif()
if(LZ4_FOUND)
    target_link_libraries(delila_net PRIVATE ${LZ4_LIBRARIES})
endif()
if(XXHASH_FOUND)
    target_link_libraries(delila_net PRIVATE ${XXHASH_LIBRARIES})
endif()

target_compile_features(delila_net PUBLIC cxx_std_17)

# Add optional compiler definitions
if(ZMQ_FOUND)
    target_compile_definitions(delila_net PRIVATE ${ZMQ_CFLAGS_OTHER})
endif()

# Install headers (if they exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    install(DIRECTORY include/
        DESTINATION include/delila/net
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()