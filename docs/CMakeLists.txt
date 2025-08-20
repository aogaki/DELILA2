# Documentation CMakeLists.txt for DELILA2
# Doxygen + PlantUML integration

cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(Doxygen QUIET)

# Documentation option
option(BUILD_DOCUMENTATION "Build API documentation with Doxygen" OFF)

if(BUILD_DOCUMENTATION)
    if(NOT DOXYGEN_FOUND)
        message(WARNING "Doxygen not found. Documentation will not be built.")
        return()
    endif()

    # Configure Doxygen paths
    set(DOXYGEN_INPUT_DIR "${CMAKE_SOURCE_DIR}")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/api")
    set(DOXYGEN_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")

    # Check if PlantUML is available
    find_program(JAVA_EXECUTABLE java)
    find_program(PLANTUML_EXECUTABLE plantuml)
    find_file(PLANTUML_JAR plantuml.jar
        PATHS
            /opt/homebrew/share/plantuml
            /usr/share/plantuml
            /usr/local/share/plantuml
            /opt/plantuml
            $ENV{PLANTUML_JAR_PATH}
        DOC "Path to PlantUML jar file"
    )

    if(PLANTUML_EXECUTABLE OR (JAVA_EXECUTABLE AND PLANTUML_JAR))
        if(PLANTUML_EXECUTABLE)
            message(STATUS "PlantUML executable found: ${PLANTUML_EXECUTABLE}")
        else()
            message(STATUS "PlantUML jar found: ${PLANTUML_JAR}")
        endif()
        set(PLANTUML_AVAILABLE TRUE)
    else()
        message(WARNING "PlantUML not found. Diagrams will not be generated.")
        set(PLANTUML_AVAILABLE FALSE)
    endif()

    # Generate PlantUML diagrams if available
    if(PLANTUML_AVAILABLE)
        # Find all .puml files
        file(GLOB PUML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/diagrams/*.puml")
        
        set(GENERATED_DIAGRAMS "")
        foreach(PUML_FILE ${PUML_FILES})
            get_filename_component(DIAGRAM_NAME ${PUML_FILE} NAME_WE)
            set(PNG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/diagrams/${DIAGRAM_NAME}.png")
            
            # Add custom command to generate PNG from PlantUML
            if(PLANTUML_EXECUTABLE)
                add_custom_command(
                    OUTPUT ${PNG_FILE}
                    COMMAND ${PLANTUML_EXECUTABLE} -tpng -o ${CMAKE_CURRENT_SOURCE_DIR}/diagrams ${PUML_FILE}
                    DEPENDS ${PUML_FILE}
                    COMMENT "Generating diagram ${DIAGRAM_NAME}.png"
                    VERBATIM
                )
            else()
                add_custom_command(
                    OUTPUT ${PNG_FILE}
                    COMMAND ${JAVA_EXECUTABLE} -jar ${PLANTUML_JAR} -tpng -o ${CMAKE_CURRENT_SOURCE_DIR}/diagrams ${PUML_FILE}
                    DEPENDS ${PUML_FILE}
                    COMMENT "Generating diagram ${DIAGRAM_NAME}.png"
                    VERBATIM
                )
            endif()
            
            list(APPEND GENERATED_DIAGRAMS ${PNG_FILE})
        endforeach()

        # Custom target for diagrams
        add_custom_target(diagrams
            DEPENDS ${GENERATED_DIAGRAMS}
            COMMENT "Generating PlantUML diagrams"
        )
    endif()

    # Create output directory
    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})

    # Add custom target for documentation
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    # Make docs depend on diagrams if PlantUML is available
    if(PLANTUML_AVAILABLE)
        add_dependencies(docs diagrams)
    endif()

    # Install documentation
    install(DIRECTORY ${DOXYGEN_OUTPUT_DIR}
        DESTINATION share/doc/delila2
        COMPONENT documentation
        OPTIONAL
    )

    # Add documentation to 'all' target if explicitly requested  
    # Note: Skip adding to 'all' target to avoid dependency issues

    message(STATUS "Documentation will be generated in: ${DOXYGEN_OUTPUT_DIR}")
    message(STATUS "To build documentation: make docs")
    if(PLANTUML_AVAILABLE)
        message(STATUS "To generate diagrams only: make diagrams")
    endif()

else()
    message(STATUS "Documentation build disabled. Use -DBUILD_DOCUMENTATION=ON to enable.")
endif()

# Additional targets for convenience
if(DOXYGEN_FOUND)
    # Clean documentation
    add_custom_target(docs-clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/api
        COMMENT "Cleaning generated documentation"
    )

    # Open documentation in browser (Linux/macOS)
    if(UNIX)
        add_custom_target(docs-open
            COMMAND xdg-open ${DOXYGEN_OUTPUT_DIR}/html/index.html || open ${DOXYGEN_OUTPUT_DIR}/html/index.html
            DEPENDS docs
            COMMENT "Opening documentation in browser"
        )
    endif()
endif()

# Print helpful information
if(BUILD_DOCUMENTATION AND DOXYGEN_FOUND)
    message(STATUS "")
    message(STATUS "Documentation targets:")
    message(STATUS "  docs        - Generate complete API documentation")
    if(PLANTUML_AVAILABLE)
        message(STATUS "  diagrams    - Generate PlantUML diagrams only")
    endif()
    message(STATUS "  docs-clean  - Clean generated documentation")
    if(UNIX)
        message(STATUS "  docs-open   - Open documentation in browser")
    endif()
    message(STATUS "")
endif()