# DELILA2 System Requirements

This document describes the overall DELILA2 DAQ system requirements.

## Overview
DELILA2 is a high-performance Data Acquisition (DAQ) system for nuclear physics experiments, providing real-time data collection, processing, and storage capabilities.

## Component Libraries
- [Digitizer Library](lib/digitizer/docs/README.md) - Hardware interface and data acquisition
- [Network Library](lib/net/docs/README.md) - High-performance data transport

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
- **Linux**: Primary platform (Ubuntu 20.04+, CentOS 8+, Rocky Linux 8+)
- **macOS**: Development platform (macOS 11+ with Homebrew)
- **Hardware**: x86_64 architecture, 16GB+ RAM recommended

### 6. Dependencies

#### Core Dependencies
- **CMake**: Version 3.15 or higher
- **C++ Compiler**: GCC 7+ or Clang 8+ with C++17 support
- **pkg-config**: For dependency detection

#### Network Library Dependencies
- **ZeroMQ (libzmq)**: High-performance messaging library
- **LZ4 (liblz4)**: Fast compression library

#### Optional Dependencies
- **ROOT**: For monitoring and data visualization (6.20+)
- **CAEN Libraries**: For hardware digitizer support
  - CAEN_FELib
  - CAEN_Dig2

#### Linux Installation Commands

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake pkg-config
sudo apt install libzmq3-dev liblz4-dev
```

**CentOS/RHEL/Rocky Linux:**
```bash
sudo dnf install gcc-c++ cmake pkg-config
sudo dnf install zeromq-devel lz4-devel
```

**macOS (Homebrew):**
```bash
brew install cmake pkg-config
brew install zeromq lz4
```

## Non-Requirements
- Windows support not required
- Real-time operating system not required
- Embedded system support not required