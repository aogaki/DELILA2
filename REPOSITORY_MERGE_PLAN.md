# DELILA2 Repository Merge Plan

## Overview
This plan merges three separate repositories into one unified DELILA2 repository:
- **DELILA2** (main repository) - System/application level
- **libDELILA-digitizer** (separate repo) - Digitizer library 
- **libDELILA-net** (this current repo) - Network library

## Target Structure After Merge
```
DELILA2/ (unified repository)
├── lib/
│   ├── digitizer/              # From libDELILA-digitizer repo
│   │   ├── docs/
│   │   │   ├── REQUIREMENTS.md
│   │   │   ├── DESIGN.md
│   │   │   └── README.md
│   │   ├── include/delila/digitizer/
│   │   │   └── EventData.hpp   # Moved from net/
│   │   └── src/
│   └── net/                    # From libDELILA-net repo
│       ├── docs/
│       │   ├── REQUIREMENTS.md
│       │   ├── DESIGN.md
│       │   └── LICENSE
│       ├── include/delila/net/
│       └── src/
├── include/delila/             # Unified public headers
├── src/                        # Application source
├── tests/                      # System-level tests
├── examples/                   # Usage examples
├── CMakeLists.txt             # Builds single libDELILA.so
├── REQUIREMENTS.md            # System-level requirements
├── DESIGN.md                  # System-level architecture
└── README.md                  # Main project documentation
```

## Execution Steps

### Prerequisites
- Ensure you have access to all three repositories
- Backup all repositories before starting
- Ensure all repositories have clean working directories (commit/stash changes)

### Step 1: Prepare Main DELILA2 Repository
```bash
cd DELILA2
git checkout main
git pull origin main

# Create lib directory structure
mkdir -p lib/digitizer/docs
mkdir -p lib/digitizer/include/delila/digitizer
mkdir -p lib/digitizer/src
mkdir -p lib/net/docs
mkdir -p lib/net/include/delila/net
mkdir -p lib/net/src
mkdir -p include/delila/digitizer
mkdir -p include/delila/net
mkdir -p tests
mkdir -p examples

git add .
git commit -m "Create unified repository directory structure"
```

### Step 2: Merge libDELILA-digitizer Repository
```bash
cd DELILA2

# Add digitizer repo as remote (adjust path as needed)
git remote add digitizer-repo ../libDELILA-digitizer
# Or if it's a URL: git remote add digitizer-repo https://github.com/user/libDELILA-digitizer.git
git fetch digitizer-repo

# Merge with subtree strategy
git subtree add --prefix=lib/digitizer digitizer-repo main --squash

# Move documents to avoid conflicts
cd lib/digitizer
if [ -f REQUIREMENTS.md ]; then mv REQUIREMENTS.md docs/REQUIREMENTS.md; fi
if [ -f DESIGN.md ]; then mv DESIGN.md docs/DESIGN.md; fi
if [ -f README.md ]; then mv README.md docs/README.md; fi
if [ -f LICENSE ]; then mv LICENSE docs/LICENSE; fi
cd ../..

git add -A
git commit -m "Reorganize digitizer documentation to docs/ subdirectory"
```

### Step 3: Merge libDELILA-net Repository
```bash
cd DELILA2

# Add net repo as remote (adjust path as needed)
git remote add net-repo ../lib/net
# Or if it's a URL: git remote add net-repo https://github.com/user/libDELILA-net.git
git fetch net-repo

# Merge with subtree strategy
git subtree add --prefix=lib/net net-repo main --squash

# Move documents to avoid conflicts
cd lib/net
if [ -f REQUIREMENTS.md ]; then mv REQUIREMENTS.md docs/REQUIREMENTS.md; fi
if [ -f DESIGN.md ]; then mv DESIGN.md docs/DESIGN.md; fi
if [ -f README.md ]; then mv README.md docs/README.md; fi
if [ -f LICENSE ]; then mv LICENSE docs/LICENSE; fi
cd ../..

git add -A
git commit -m "Reorganize network documentation to docs/ subdirectory"
```

### Step 4: Move EventData.hpp to Correct Location
```bash
cd DELILA2

# Move EventData.hpp from net to digitizer (since it's digitizer-specific)
if [ -f lib/net/EventData.hpp ]; then
    mv lib/net/EventData.hpp lib/digitizer/include/delila/digitizer/EventData.hpp
fi

# Update any includes in net library to reference digitizer location
find lib/net -name "*.hpp" -o -name "*.cpp" | xargs sed -i.bak 's|#include "EventData.hpp"|#include <delila/digitizer/EventData.hpp>|g'
find lib/net -name "*.bak" -delete

git add -A
git commit -m "Move EventData.hpp to digitizer library and fix include paths"
```

### Step 5: Create Unified Build System
```bash
cd DELILA2

# Create main CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.15)
project(DELILA2 VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find dependencies
find_package(PkgConfig REQUIRED)

# ZeroMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# LZ4
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

# xxHash
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED libxxhash)

# Protocol Buffers
find_package(Protobuf REQUIRED)

# Add subdirectories
add_subdirectory(lib/digitizer)
add_subdirectory(lib/net)

# Create unified libDELILA.so
add_library(DELILA SHARED)
target_link_libraries(DELILA 
    PUBLIC delila_digitizer delila_net
    PRIVATE ${ZMQ_LIBRARIES} ${LZ4_LIBRARIES} ${XXHASH_LIBRARIES} ${Protobuf_LIBRARIES}
)

target_include_directories(DELILA
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ZMQ_INCLUDE_DIRS} ${LZ4_INCLUDE_DIRS} 
        ${XXHASH_INCLUDE_DIRS} ${Protobuf_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(DELILA PRIVATE ${ZMQ_CFLAGS_OTHER})

# Set library properties
set_target_properties(DELILA PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/delila/delila.hpp"
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Add examples
add_subdirectory(examples)

# Install configuration
install(TARGETS DELILA
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/delila
)

install(DIRECTORY include/delila
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DELILA2Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2Config.cmake"
    INSTALL_DESTINATION lib/cmake/DELILA2
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DELILA2ConfigVersion.cmake"
    DESTINATION lib/cmake/DELILA2
)
EOF

git add CMakeLists.txt
git commit -m "Add unified build system for libDELILA.so"
```

### Step 6: Create Placeholder Component CMakeLists.txt
```bash
cd DELILA2

# Create digitizer CMakeLists.txt if it doesn't exist
if [ ! -f lib/digitizer/CMakeLists.txt ]; then
cat > lib/digitizer/CMakeLists.txt << 'EOF'
# Digitizer library
add_library(delila_digitizer STATIC
    # Add source files here
    # src/digitizer.cpp
)

target_include_directories(delila_digitizer
    PUBLIC include
    PRIVATE src
)

target_compile_features(delila_digitizer PUBLIC cxx_std_17)
EOF
fi

# Create net CMakeLists.txt if it doesn't exist
if [ ! -f lib/net/CMakeLists.txt ]; then
cat > lib/net/CMakeLists.txt << 'EOF'
# Network library
add_library(delila_net STATIC
    # Add source files here
    # src/serialization.cpp
    # src/connection.cpp
    # src/config.cpp
)

target_include_directories(delila_net
    PUBLIC include
    PRIVATE src
)

target_link_libraries(delila_net
    PUBLIC delila_digitizer
    PRIVATE ${ZMQ_LIBRARIES} ${LZ4_LIBRARIES} ${XXHASH_LIBRARIES}
)

target_compile_features(delila_net PUBLIC cxx_std_17)
EOF
fi

# Create tests CMakeLists.txt
cat > tests/CMakeLists.txt << 'EOF'
# Find GoogleTest
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Unit tests
add_executable(delila_unit_tests
    # Add test files here when they exist
    # unit/serialization/test_binary_serializer.cpp
    # unit/transport/test_connection.cpp
)

target_link_libraries(delila_unit_tests
    DELILA
    GTest::gtest_main
    GTest::gmock
)

# Integration tests  
add_executable(delila_integration_tests
    # Add integration test files here when they exist
    # integration/test_multi_module.cpp
)

target_link_libraries(delila_integration_tests
    DELILA
    GTest::gtest_main
)

# Benchmarks
add_executable(delila_benchmarks
    # Add benchmark files here when they exist
    # benchmarks/bench_serialization.cpp
)

target_link_libraries(delila_benchmarks
    DELILA
    benchmark::benchmark
)

# Register tests
add_test(NAME UnitTests COMMAND delila_unit_tests)
add_test(NAME IntegrationTests COMMAND delila_integration_tests)
EOF

# Create examples CMakeLists.txt
cat > examples/CMakeLists.txt << 'EOF'
# Example applications
add_executable(basic_usage
    # Add example files here when they exist
    # basic_usage.cpp
)

target_link_libraries(basic_usage DELILA)
EOF

git add -A
git commit -m "Add component build configurations"
```

### Step 7: Create System-Level Documentation
```bash
cd DELILA2

# Create system-level REQUIREMENTS.md
cat > REQUIREMENTS.md << 'EOF'
# DELILA2 System Requirements

This document describes the overall DELILA2 DAQ system requirements.

## Overview
DELILA2 is a high-performance Data Acquisition (DAQ) system for nuclear physics experiments, providing real-time data collection, processing, and storage capabilities.

## Component Libraries
- [Digitizer Library](lib/digitizer/docs/REQUIREMENTS.md) - Hardware interface and data acquisition
- [Network Library](lib/net/docs/REQUIREMENTS.md) - High-performance data transport

## System Integration Requirements

### 1. Performance Requirements
- **Data Rate**: Support up to 500 MB/s sustained data acquisition
- **Latency**: Real-time processing with minimal buffering delays
- **Reliability**: 99.9% uptime during data acquisition runs

### 2. System Architecture
- **Modular Design**: Pluggable components for different detector configurations
- **Scalability**: Support 1-50 digitizer modules per system
- **Distributed Processing**: Multi-node processing capability

### 3. Data Management
- **Real-time Processing**: Online analysis and monitoring
- **Data Storage**: ROOT file format with metadata
- **Data Integrity**: Checksums and sequence validation

### 4. User Interface
- **Web Interface**: Real-time monitoring and control
- **Configuration Management**: Centralized system configuration
- **Logging**: Comprehensive system and error logging

### 5. Platform Support
- **Linux**: Primary platform (Ubuntu 20.04+, CentOS 8+)
- **Hardware**: x86_64 architecture, 16GB+ RAM recommended

## Non-Requirements
- Windows support not required
- Real-time operating system not required
- Embedded system support not required
EOF

# Create system-level DESIGN.md
cat > DESIGN.md << 'EOF'
# DELILA2 System Architecture

This document describes the overall DELILA2 system architecture and component integration.

## Overview
DELILA2 follows a modular architecture with separate libraries for different functional areas, unified into a single deployment package.

## Component Architecture
- [Digitizer Library Design](lib/digitizer/docs/DESIGN.md) - Hardware abstraction and data structures
- [Network Library Design](lib/net/docs/DESIGN.md) - High-performance networking and serialization

## System Integration

### Module Communication Flow
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Digitizer  │───▶│   Network   │───▶│   Storage   │
│   Modules   │    │  Transport  │    │  & Analysis │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────┐
│              Control & Monitoring System            │
└─────────────────────────────────────────────────────┘
```

### Library Dependencies
- **Network Library** depends on **Digitizer Library** (for EventData structures)
- **Application Layer** uses both libraries through unified libDELILA.so
- **External Dependencies**: ZeroMQ, LZ4, xxHash, Protocol Buffers

### Build System
- **Single Library**: All components build into libDELILA.so
- **CMake**: Unified build system with component subdirectories
- **Testing**: Integrated unit tests, integration tests, and benchmarks
- **Installation**: Standard CMake install with pkg-config support

### Configuration Management
- **Layered Configuration**: Command line > Environment > File > Database > Defaults
- **Format Support**: JSON files, environment variables, database storage
- **Validation**: Comprehensive configuration validation and error reporting

## Deployment Architecture
- **Single Binary**: Complete DAQ system in one executable
- **Plugin Architecture**: Runtime loading of detector-specific modules
- **Distributed Setup**: Multi-node deployment with network communication
- **Container Support**: Docker/Podman deployment capability
EOF

# Create unified main header
mkdir -p include/delila
cat > include/delila/delila.hpp << 'EOF'
#ifndef DELILA_HPP
#define DELILA_HPP

/**
 * @file delila.hpp
 * @brief Main header for DELILA2 unified library
 * 
 * This header provides access to all DELILA2 components:
 * - Digitizer library for hardware interface
 * - Network library for data transport
 */

// Digitizer library
#include <delila/digitizer/EventData.hpp>

// Network library  
#include <delila/net/serialization.hpp>
#include <delila/net/connection.hpp>
#include <delila/net/config.hpp>

/**
 * @namespace DELILA
 * @brief Main namespace for all DELILA2 components
 */
namespace DELILA {

/**
 * @brief Get library version information
 * @return Version string in format "major.minor.patch"
 */
const char* getVersion();

/**
 * @brief Initialize DELILA2 library
 * @param config_path Optional path to configuration file
 * @return true if initialization successful
 */
bool initialize(const char* config_path = nullptr);

/**
 * @brief Shutdown DELILA2 library and cleanup resources
 */
void shutdown();

}  // namespace DELILA

#endif  // DELILA_HPP
EOF

git add -A
git commit -m "Add system-level documentation and unified header"
```

### Step 8: Clean Up and Finalize
```bash
cd DELILA2

# Remove temporary remote references
git remote remove digitizer-repo 2>/dev/null || true
git remote remove net-repo 2>/dev/null || true

# Create .gitignore for build artifacts
cat > .gitignore << 'EOF'
# Build directories
build/
cmake-build-*/

# IDE files
.vscode/
.idea/
*.swp
*.swo

# Compiled files
*.o
*.so
*.a
*.dylib

# Test results
Testing/
test_results/

# Package files
*.deb
*.rpm
*.tar.gz

# OS files
.DS_Store
Thumbs.db
EOF

git add .gitignore
git commit -m "Add .gitignore for build artifacts"

# Final commit
git add -A
git commit -m "Complete repository merge into unified DELILA2

This commit completes the merge of separate digitizer and network 
repositories into a unified DELILA2 repository with:

- Unified build system producing single libDELILA.so
- Organized documentation in subdirectories
- System-level architecture documentation  
- Complete CMake configuration with dependencies
- Proper header organization and include paths

🤖 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>"

echo "Repository merge completed successfully!"
echo "Next steps:"
echo "1. Test the build: mkdir build && cd build && cmake .. && make"
echo "2. Verify all components compile correctly"
echo "3. Run any existing tests"
echo "4. Update any CI/CD configurations"
echo "5. Update README.md with new structure"
```

## Post-Merge Verification

After executing the merge, verify everything works:

```bash
# Test build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Check library was created
ls -la libDELILA.so*

# Run tests (when they exist)
make test

# Check installation
sudo make install
```

## Rollback Plan (if needed)

If anything goes wrong, you can rollback:

```bash
cd DELILA2
git log --oneline -10  # Find commit before merge
git reset --hard <commit-hash-before-merge>
```

## Notes
- Adjust repository paths in Step 2 and 3 based on your actual repository locations
- Ensure all repositories have clean working directories before starting
- The plan assumes standard git repository structures
- Test the build after merge to ensure all dependencies are correctly configured

Copy this file to your DELILA2 root directory and execute the steps in order.