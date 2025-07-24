# Examples for DELILA2
cmake_minimum_required(VERSION 3.15)

# Collect example source files
file(GLOB EXAMPLE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Create executables for each example
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} DELILA)
endforeach()

# If no examples exist, create a placeholder comment
if(NOT EXAMPLE_SOURCES)
    # No example source files found
    # Add example applications here when available
endif()