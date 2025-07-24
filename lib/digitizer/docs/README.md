# libDELILA-digitizer

A comprehensive C++ library for interfacing with CAEN digitizers, supporting multiple firmware types and providing high-performance data acquisition capabilities.

## 🚀 Features

### Multi-Firmware Support
- **PSD1**: Pulse Shape Discrimination firmware for older digitizers
- **PSD2**: Pulse Shape Discrimination firmware for newer digitizers  
- **PHA1**: Pulse Height Analysis firmware for older digitizers
- **Automatic Detection**: Firmware type automatically detected from device tree

### Supported Hardware
- **DT5725** (X725): 8-channel digitizer
- **DT5730** (X730): 16-channel digitizer
- **Scalable Architecture**: Easy to extend for additional models

### Advanced Features
- ✅ **Fine Timestamp Support**: Sub-nanosecond timing precision
- ✅ **Multi-threaded Processing**: Efficient CPU utilization
- ✅ **Robust Error Handling**: Comprehensive error recovery
- ✅ **Flexible Configuration**: JSON-based device tree configuration
- ✅ **Real-time Processing**: Low-latency data acquisition

## 📋 Requirements

### Dependencies

#### Core CAEN Libraries (Required)
- **CAEN FELib**: CAEN Front-End Library
- **caen-dig1**: CAEN digitizer library for older firmware types (PSD1, PHA1)
- **caen-dig2**: CAEN digitizer library for newer firmware types (PSD2, PHA2)

> ⚠️ **Important**: Both `caen-dig1` and `caen-dig2` libraries are **required** even if you only plan to use one firmware type. The library automatically selects the appropriate backend based on your digitizer's firmware.

#### Additional Dependencies
- **nlohmann/json**: JSON library for C++
- **C++17 or later**: Modern C++ standard support

### Supported Platforms
- Linux (primary development platform)
- Compatible with CAEN digitizer hardware

## 🔧 Building

### Prerequisites
First, ensure you have installed the required CAEN libraries:

```bash
# Install CAEN digitizer libraries
# These are typically provided by CAEN as part of their digitizer software package
# Make sure both libraries are installed:
# - caen-dig1 (for PSD1, PHA1 firmware)
# - caen-dig2 (for PSD2, PHA2 firmware)

# Example installation paths (adjust for your system):
# /usr/local/lib/libcaen-dig1.so
# /usr/local/lib/libcaen-dig2.so
```

### Build Steps
```bash
# Clone the repository
git clone <repository-url>
cd libDELILA-digitizer

# Build the library
mkdir build
cd build
cmake ..
make
```

### Installation Verification
To verify that all dependencies are correctly installed:
```bash
# Check if CAEN libraries are available
ldconfig -p | grep caen
# Should show both caen-dig1 and caen-dig2
```

## 🎯 Quick Start

### Basic Usage

```cpp
#include "DigitizerFactory.hpp"
#include "ConfigurationManager.hpp"

using namespace DELILA::Digitizer;

int main() {
    // Load configuration
    ConfigurationManager config("digitizer.conf");
    
    // Create digitizer instance
    auto digitizer = DigitizerFactory::CreateDigitizer(config);
    
    // Initialize and configure
    digitizer->Initialize(config);
    digitizer->Configure();
    
    // Start data acquisition
    digitizer->StartAcquisition();
    
    // Process data
    while (running) {
        auto eventData = digitizer->GetEventData();
        // Process your events here
    }
    
    // Stop acquisition
    digitizer->StopAcquisition();
    
    return 0;
}
```

### Configuration File Example

```ini
# Connection settings
URL dig1://caen.internal/usb?link_num=0
ModID 1
Debug false
Threads 4

# Basic board configuration
/par/startmode START_MODE_SW
/par/EventAggr 1023
/par/reclen 512
/par/waveforms false

# Channel configuration
/ch/0..7/par/ch_enabled TRUE
/ch/0..7/par/ch_dcoffset 20
/ch/0..7/par/ch_threshold 500
/ch/0..7/par/ch_polarity POLARITY_NEGATIVE

# PSD-specific settings (for PSD firmware)
/ch/0..7/par/ch_gate 400
/ch/0..7/par/ch_gateshort 80
/ch/0..7/par/ch_gatepre 50
/ch/0..7/par/ch_cfd_delay 4
```

## 📊 Firmware Types

### PSD (Pulse Shape Discrimination)
- **Use Case**: Neutron/gamma discrimination
- **Features**: Long/short gate integration, CFD timing
- **Models**: PSD1 (older), PSD2 (newer)

### PHA (Pulse Height Analysis)
- **Use Case**: Energy spectroscopy
- **Features**: Peak detection, energy measurement
- **Models**: PHA1 (older), PHA2 (newer)

## 🔍 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│  DigitizerFactory  │  ConfigurationManager  │  EventData   │
├─────────────────────────────────────────────────────────────┤
│          IDigitizer Interface (Abstract)                    │
├─────────────────────────────────────────────────────────────┤
│     Digitizer1     │            Digitizer2                 │
├─────────────────────────────────────────────────────────────┤
│  PSD1Decoder │ PHA1Decoder │ PSD2Decoder │ QDC1Decoder    │
├─────────────────────────────────────────────────────────────┤
│                    CAEN FELib                               │
├─────────────────────────────────────────────────────────────┤
│                 Hardware Layer                              │
└─────────────────────────────────────────────────────────────┘
```

## 📈 Performance

- **Multi-threaded Design**: Separate threads for data acquisition and processing
- **Lock-free Queues**: Efficient data transfer between threads
- **Optimized Memory Usage**: Minimal memory allocations during runtime
- **Real-time Capable**: Sub-microsecond processing latency

## 🛠️ Advanced Features

### Fine Timestamp Configuration
```cpp
// Enable fine timestamp for high-precision timing
digitizer->EnableFineTimestamp();
```

### Multi-board Synchronization
```cpp
// Configure multiple digitizers for synchronized acquisition
// (Implementation depends on your specific setup)
```

### Custom Event Processing
```cpp
// Process events with custom logic
auto events = digitizer->GetEventData();
for (const auto& event : *events) {
    // Access timing information
    double timestamp = event->GetTimestamp();
    
    // Access energy information (PHA)
    double energy = event->GetEnergy();
    
    // Access waveform data (if enabled)
    auto waveform = event->GetWaveform();
}
```

## 🔧 Configuration Options

### Connection Parameters
- `URL`: Digitizer connection string
- `ModID`: Module identification number
- `Debug`: Enable debug output
- `Threads`: Number of processing threads

### Digitizer-Specific Parameters
Configuration parameters vary significantly based on your digitizer model and firmware type. Please refer to the appropriate configuration file for your setup:

#### Available Configuration Files
- **`PSD1.conf`**: For PSD1 firmware (older PSD digitizers)
- **`PSD2.conf`**: For PSD2 firmware (newer PSD digitizers)
- **`PHA1.conf`**: For PHA1 firmware (older PHA digitizers)
- **`dig1.conf`**: General digitizer configuration template

#### Example: PSD1 Configuration
```ini
# PSD-specific timing parameters
/par/reclen 512              # Record length in samples
/par/startmode START_MODE_SW # Software start mode
/par/EventAggr 1023         # Event aggregation

# PSD-specific channel parameters
/ch/0..7/par/ch_gate 400        # Long gate: 400ns 
/ch/0..7/par/ch_gateshort 80    # Short gate: 80ns
/ch/0..7/par/ch_gatepre 50      # Pre-gate: 50ns
/ch/0..7/par/ch_cfd_delay 4     # CFD delay: 4ns
```

#### Example: PHA1 Configuration
```ini
# PHA-specific parameters (simpler than PSD)
/ch/0..7/par/ch_enabled TRUE
/ch/0..7/par/ch_dcoffset 20
/ch/0..7/par/ch_threshold 500
/ch/0..7/par/ch_polarity POLARITY_NEGATIVE
```

> 💡 **Tip**: Start with the configuration file that matches your digitizer's firmware type, then customize parameters based on your specific requirements and hardware capabilities.


## 🐛 Troubleshooting

### Common Issues
1. **Connection Failed**: Check USB connection and permissions
2. **Configuration Error**: Verify parameter names in config file
3. **Performance Issues**: Adjust thread count and buffer sizes
4. **Data Loss**: Check system resources and processing speed

### Debug Mode
Enable debug output for detailed information:
```ini
Debug true
```

## 🤝 Contributing

We welcome contributions! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## 📄 License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.

## 📞 Support

For questions and support:
- Create an issue on GitHub
- Check the documentation in `docs/`
- Review completed features in `COMPLETED.md`

## 🔄 Version History

- **v0.1.0-alpha**: Initial development version with PSD1 support
- **Current (v0.1.x-alpha)**: Active development with PHA1 support and fine timestamp features

> ⚠️ **Alpha Version**: This library is currently in alpha stage. APIs may change and features are still being developed and tested.

---

*Built with ❤️ for the scientific community*