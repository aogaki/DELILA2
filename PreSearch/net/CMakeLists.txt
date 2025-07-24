cmake_minimum_required(VERSION 3.16)
project(delila_net_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find gRPC
pkg_check_modules(gRPC REQUIRED grpc++)
find_program(gRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT gRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found")
endif()

# Find ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)

# Find JSON library
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${gRPC_INCLUDE_DIRS})

# Generate protobuf files
set(PROTO_FILES
    proto/messages.proto
)

# Generate protobuf C++ files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Generate gRPC C++ files
function(grpc_generate_cpp SRCS HDRS)
    if(NOT ARGN)
        message(SEND_ERROR "Error: grpc_generate_cpp() called without any proto files")
        return()
    endif()

    set(${SRCS})
    set(${HDRS})
    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)
        get_filename_component(FIL_DIR ${ABS_FIL} DIRECTORY)
        
        list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/proto/${FIL_WE}.grpc.pb.cc")
        list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/proto/${FIL_WE}.grpc.pb.h")
        
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/proto/${FIL_WE}.grpc.pb.cc"
                   "${CMAKE_CURRENT_BINARY_DIR}/proto/${FIL_WE}.grpc.pb.h"
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                 --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                 -I${CMAKE_CURRENT_SOURCE_DIR}
                 --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN}
                 ${ABS_FIL}
            DEPENDS ${ABS_FIL}
            COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
            VERBATIM
        )
    endforeach()
    
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

# Generate gRPC files
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_FILES})

# Common library
add_library(test_common
    src/EventDataBatch.cpp
    src/StatsCollector.cpp
    src/ConfigLoader.cpp
    src/Common.cpp
    src/Logger.cpp
    src/MemoryMonitor.cpp
    src/EventGenerator.cpp
    src/SequenceValidator.cpp
    src/HtmlReportGenerator.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(test_common
    ${Protobuf_LIBRARIES}
    ${gRPC_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Transport implementations
add_library(transports
    src/ZmqTransport.cpp
    src/GrpcTransport.cpp
    src/TransportFactory.cpp
)

target_link_libraries(transports
    test_common
    ${Protobuf_LIBRARIES}
    ${gRPC_LIBRARIES}
    ${ZMQ_LIBRARIES}
)

# Executables
add_executable(data_sender src/DataSender.cpp)
add_executable(data_hub src/DataHub.cpp)
add_executable(data_receiver src/DataReceiver.cpp)

# Link executables
target_link_libraries(data_sender test_common transports)
target_link_libraries(data_hub test_common transports)
target_link_libraries(data_receiver test_common transports)

# Install targets
install(TARGETS data_sender data_hub data_receiver
    RUNTIME DESTINATION bin
)

# Custom target to generate proto files
add_custom_target(proto_gen DEPENDS ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS})