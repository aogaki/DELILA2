# DELILA2 System Architecture

This document describes the overall DELILA2 system architecture and component integration.

## Overview
DELILA2 follows a modular architecture with separate libraries for different functional areas, unified into a single deployment package.

## Component Architecture
- [Digitizer Library Design](lib/digitizer/docs/README.md) - Hardware abstraction and data structures
- [Network Library Design](lib/net/docs/README.md) - High-performance networking and serialization

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
- **Network Library** depends on **Digitizer Library** (for EventData and MinimalEventData structures)
- **Application Layer** uses both libraries through unified libDELILA.so
- **External Dependencies**: ZeroMQ, LZ4, Protocol Buffers, ROOT

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