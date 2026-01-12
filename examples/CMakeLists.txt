# examples/CMakeLists.txt
# Example executables for DELILA2 components

# Emulator executable
add_executable(delila_emulator emulator_main.cpp)
target_link_libraries(delila_emulator DELILA)

# SimpleMerger executable
add_executable(delila_merger merger_main.cpp)
target_link_libraries(delila_merger DELILA)

# FileWriter executable
add_executable(delila_writer writer_main.cpp)
target_link_libraries(delila_writer DELILA)

# MonitorROOT executable (only if ROOT is available)
if(HAS_ROOT)
    add_executable(delila_monitor monitor_main.cpp)
    target_link_libraries(delila_monitor DELILA)
endif()
