# Digitizer2 Basic Example Guide

## Overview

The `digitizer2_basic_example` demonstrates how to interface with CAEN Digitizer 2 hardware (such as the V2740 series) using the DELILA2 library. This example provides a complete workflow from configuration loading to data acquisition and graceful shutdown.

## Features

- **Configuration Management**: Load settings from a file
- **Hardware Initialization**: Establish connection to Digitizer2 hardware
- **Device Information**: Display firmware type, module number, and device tree
- **Data Acquisition**: Continuous event readout with batch statistics
- **Graceful Shutdown**: Signal handling for clean termination (Ctrl+C)
- **Performance Metrics**: Track event rates, batch sizes, and acquisition time

## Prerequisites

### Hardware
- CAEN Digitizer 2 (e.g., V2740, V2745)
- Network connection to the digitizer
- Digitizer must be powered on and accessible

### Software
- DELILA2 library installed
- CAEN FELib library installed (libCAEN_FELib.so/dylib)
- CAEN Dig2 library installed (libCAEN_Dig2.so/dylib)
- Configuration file (dig2.conf)

## Configuration File

The example uses a configuration file (default: `dig2.conf`) with the following structure:

```conf
# Connection settings
URL dig2://172.18.4.56    # Digitizer network address
ModID 3                    # Module identifier

# Global parameters
/par/StartSource SWcmd
/par/GPIOMode Run

# Channel configuration (channels 0-31)
/ch/0..31/par/ChEnable True
/ch/0..31/par/DCOffset 20
/ch/0..31/par/PulsePolarity Negative
/ch/0..31/par/TriggerThr 500

# Gate settings for PSD mode
/ch/0..31/par/GateLongLengthT 400
/ch/0..31/par/GateShortLengthT 100

# Waveform trigger (disabled by default)
/ch/0..31/par/WaveTriggerSource Disabled
```

### Key Configuration Parameters

| Parameter | Description | Example Value |
|-----------|-------------|---------------|
| `URL` | Network address of digitizer | `dig2://172.18.4.56` |
| `ModID` | Module identifier (0-255) | `3` |
| `StartSource` | Acquisition start trigger | `SWcmd`, `SIn_Level` |
| `ChEnable` | Enable/disable channel | `True`, `False` |
| `DCOffset` | DC offset in percent | `0`-`100` |
| `PulsePolarity` | Signal polarity | `Positive`, `Negative` |
| `TriggerThr` | Trigger threshold (LSB) | `500` |
| `GateLongLengthT` | Long gate length (samples) | `400` |
| `GateShortLengthT` | Short gate length (samples) | `100` |

## Building

From the examples directory:

```bash
cd examples/build
cmake ..
make digitizer2_basic_example
```

Or from the main project build directory:

```bash
cd build
make digitizer2_basic_example
```

The executable will be located at:
```
build/examples/digitizer2_basic_example
```

## Running

### Basic Usage

```bash
./digitizer2_basic_example [config_file]
```

If no config file is specified, it defaults to `dig2.conf`.

### Example Run

```bash
cd /path/to/DELILA2/lib/digitizer  # Where dig2.conf is located
../../build/examples/digitizer2_basic_example dig2.conf
```

### Expected Output

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

[Device Information]
    Firmware Type: 2
    Module Number: 3

[4] Configuring digitizer...
    Digitizer configured successfully

[5] Starting data acquisition...
    Acquisition started successfully

[6] Sending software trigger for testing...
    Software trigger sent

[7] Starting main acquisition loop...
    (Waiting for events...)

Batch #1 | Events: 5
  Module: 3 | Channel: 0 | Timestamp: 1234567890 ns | Energy: 2500 | Short: 2400
  Module: 3 | Channel: 1 | Timestamp: 1234568000 ns | Energy: 3000 | Short: 2900
  Module: 3 | Channel: 2 | Timestamp: 1234568100 ns | Energy: 2800 | Short: 2700

^C
Received SIGINT (Ctrl+C), stopping acquisition...

[8] Stopping acquisition...
    Acquisition stopped successfully

========================================
  Final Statistics
========================================
Total events:    150
Total batches:   30
Elapsed time:    10 seconds
Average rate:    15.0 events/second
Avg batch size:  5.0 events/batch

Example completed successfully!
```

## Code Structure

The example follows this workflow:

### 1. Configuration Loading
```cpp
ConfigurationManager config;
auto load_result = config.LoadFromFile(config_file);
```

### 2. Digitizer Creation
```cpp
auto digitizer = std::make_unique<Digitizer2>();
```

### 3. Initialization
```cpp
if (!digitizer->Initialize(config)) {
    // Handle error
}
```

### 4. Configuration
```cpp
if (!digitizer->Configure()) {
    // Handle error
}
```

### 5. Start Acquisition
```cpp
if (!digitizer->StartAcquisition()) {
    // Handle error
}
```

### 6. Data Readout Loop
```cpp
while (g_running) {
    auto events = digitizer->GetEventData();
    if (events && !events->empty()) {
        // Process events
        for (const auto& event : *events) {
            // Access event data:
            // event->module, event->channel,
            // event->timeStampNs, event->energy, etc.
        }
    }
}
```

### 7. Stop Acquisition
```cpp
digitizer->StopAcquisition();
```

## Event Data Structure

Each event returned by `GetEventData()` contains:

```cpp
struct EventData {
    uint8_t module;           // Module identifier
    uint8_t channel;          // Channel number (0-31)
    uint64_t timeStampNs;     // Timestamp in nanoseconds
    uint16_t energy;          // Long gate integral (energy)
    uint16_t energyShort;     // Short gate integral
    uint16_t flags;           // Event flags
    uint16_t waveformLength;  // Number of waveform samples
    std::vector<uint16_t> waveform;  // Waveform data (if enabled)
};
```

## Troubleshooting

### "Failed to load configuration"
- Check that the config file path is correct
- Verify the config file syntax
- Ensure all required parameters are present

### "Failed to initialize digitizer"
- Verify digitizer is powered on
- Check network connectivity to the digitizer IP
- Ensure CAEN libraries are installed (libCAEN_FELib, libCAEN_Dig2)
- Verify the URL in the config file is correct
- Check that no other program is using the digitizer

### "Failed to configure digitizer"
- Review configuration parameters for validity
- Check firmware compatibility with settings
- Ensure channels are within valid range (0-31 for V2740)

### No Events Received
- Check trigger configuration in config file
- Try sending a software trigger: `SendSWTrigger()`
- Verify signal connections to digitizer inputs
- Check trigger threshold settings

### Library Not Found Errors (macOS)
```
dyld: Library not loaded: libCAEN_FELib.dylib
```

**Solution**: Set library path environment variable:
```bash
export DYLD_LIBRARY_PATH=/path/to/caen/libraries:$DYLD_LIBRARY_PATH
./digitizer2_basic_example
```

### Library Not Found Errors (Linux)
```
error while loading shared libraries: libCAEN_FELib.so
```

**Solution**: Set library path:
```bash
export LD_LIBRARY_PATH=/path/to/caen/libraries:$LD_LIBRARY_PATH
./digitizer2_basic_example
```

Or add library path to ldconfig:
```bash
sudo echo "/path/to/caen/libraries" > /etc/ld.so.conf.d/caen.conf
sudo ldconfig
```

## Integration into Your Application

To use Digitizer2 in your own application:

1. **Include headers**:
```cpp
#include <delila/digitizer/Digitizer2.hpp>
#include <delila/digitizer/ConfigurationManager.hpp>
```

2. **Link libraries** (CMakeLists.txt):
```cmake
target_link_libraries(your_app
    PRIVATE
        DELILA2::DELILA
)
```

3. **Follow the workflow**:
   - Load configuration
   - Initialize digitizer
   - Configure
   - Start acquisition
   - Read events in loop
   - Stop acquisition

## See Also

- [Digitizer2.hpp](../lib/digitizer/include/Digitizer2.hpp) - API documentation
- [IDigitizer.hpp](../lib/digitizer/include/IDigitizer.hpp) - Interface definition
- [ConfigurationManager.hpp](../lib/digitizer/include/ConfigurationManager.hpp) - Configuration API
- [dig2.conf](../lib/digitizer/dig2.conf) - Example configuration file
- [CLAUDE.md](../CLAUDE.md) - TDD guidelines for DELILA2 development

## License

This example is part of the DELILA2 project. See the main project LICENSE file for details.
