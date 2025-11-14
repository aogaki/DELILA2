# DELILA2 Examples

This directory contains example programs demonstrating how to use the DELILA2 library after it has been installed.

## Prerequisites

Before building these examples, you must have:

1. **DELILA2 installed** (typically in `/usr/local`)
2. **Dependencies installed**:
   - ZeroMQ (libzmq)
   - LZ4 compression library
   - (Optional) CAEN FELib for digitizer examples

## Building the Examples

These examples are designed to be built independently after DELILA2 is installed:

```bash
cd examples
mkdir build
cd build
cmake ..
make
```

## Available Examples

### basic_example
Demonstrates the fundamental usage of DELILA2:
- Creating a `DELILA::Digitizer::Digitizer` instance
- Working with `EventData` structures
- Using the `DataType` enum
- Library initialization and shutdown

Run with:
```bash
./basic_example
```

### digitizer2_basic_example
A complete example showing how to use the Digitizer2 interface for CAEN Digitizer 2 hardware:
- Loading configuration from file (dig2.conf)
- Initializing and configuring Digitizer2
- Starting data acquisition
- Reading event data in a loop with statistics
- Graceful shutdown with Ctrl+C handling
- Displaying device information and acquisition statistics

**Requirements**: CAEN Digitizer 2 hardware (e.g., V2740) connected via network

Run with:
```bash
./digitizer2_basic_example [config_file]
# Default config file is dig2.conf
```

Example output:
```
========================================
  DELILA2 Digitizer2 Basic Example
========================================
Press Ctrl+C to stop acquisition

Configuration file: dig2.conf

[1] Loading configuration...
    Configuration loaded successfully

[2] Creating Digitizer2 instance...
    Digitizer2 instance created

[3] Initializing digitizer...
    Digitizer initialized successfully

...
```

### simple_digitizer_test
Legacy digitizer test using the Digitizer class (PSD2 firmware):
- Demonstrates keyboard input handling for graceful shutdown
- Shows event rate statistics
- Uses PSD2.conf configuration

Run with:
```bash
./simple_digitizer_test [config_file]
# Default config file is PSD2.conf
```

## Example Structure

Each example follows a similar pattern:

1. Include the DELILA2 umbrella header: `#include <delila/delila.hpp>`
2. Initialize the library: `DELILA::initialize()`
3. Create and use DELILA2 components
4. Shutdown the library: `DELILA::shutdown()`

## Adding New Examples

To add a new example:

1. Create your `.cpp` file in this directory
2. Add it to the `CMakeLists.txt`:
   ```cmake
   add_executable(your_example your_example.cpp)
   target_link_libraries(your_example 
       PRIVATE 
           DELILA2::DELILA
           ${ZMQ_LIBRARIES}
           ${LZ4_LIBRARIES}
   )
   ```
3. Update the install section if needed

## Notes

- These examples are designed to work with an installed DELILA2 library
- They demonstrate best practices for using DELILA2 in your own projects
- Without actual digitizer hardware, some functionality will be simulated