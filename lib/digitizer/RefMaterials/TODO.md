# TODO: Future Development

## Current Status
All major development tasks have been completed successfully. The library now supports multiple firmware types (PSD1, PSD2, PHA1) with automatic decoder selection, improved reliability, and better performance.

**See COMPLETED.md for a detailed record of all finished tasks.**

## 🎯 Future Enhancements (Optional)

### Potential Future Improvements
- [ ] Add support for additional firmware types if they become available
- [ ] Implement configuration validation for firmware-specific parameters
- [ ] Add performance metrics and monitoring capabilities
- [ ] Enhance error recovery mechanisms
- [ ] Add support for multi-board synchronization features

### Documentation Improvements
- [ ] Update user manual with new firmware support information
- [ ] Add examples for different firmware types
- [ ] Create troubleshooting guide for common issues

### Testing Enhancements
- [ ] Add unit tests for decoder implementations
- [ ] Create integration tests for firmware switching
- [ ] Add performance benchmarking suite

## 📚 Documentation
- **COMPLETED.md**: Complete record of all finished development tasks
- **README.md**: General project information and usage instructions
- **TODO.md**: This file - future development planning

## 🏗️ Architecture Overview
The library currently supports:
- **PSD1**: Pulse Shape Discrimination firmware for older digitizers
- **PSD2**: Pulse Shape Discrimination firmware for newer digitizers  
- **PHA1**: Pulse Height Analysis firmware for older digitizers
- **Automatic Selection**: Based on device tree `fwtype` parameter
- **Unified Interface**: All decoders implement the same `IDecoder` interface
- **Robust Operation**: Retry logic, deadlock prevention, and efficient CPU usage

## 🚀 Current Capabilities
- ✅ Multi-firmware support with automatic decoder selection
- ✅ Improved connection reliability with retry logic
- ✅ Optimized performance with efficient CPU usage
- ✅ Deadlock-free shutdown process
- ✅ Full user control over digitizer operations
- ✅ Comprehensive error handling and logging

---
*All current planned features have been implemented successfully.*
*For completed tasks, see COMPLETED.md*