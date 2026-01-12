# DELILA2 User Guide

This guide explains how to use DELILA2 components to build a data acquisition pipeline.

## Overview

DELILA2 provides modular components that communicate via ZeroMQ (ZMQ).
Each component runs as a separate process, allowing distributed deployment across multiple machines.

### Components

| Component | Purpose | Input | Output |
|-----------|---------|-------|--------|
| **Emulator** | Generate synthetic event data | - | ZMQ PUSH |
| **DigitizerSource** | Acquire data from CAEN digitizers | Hardware | ZMQ PUSH |
| **SimpleMerger** | Merge multiple data streams | ZMQ PULL (multiple) | ZMQ PUSH |
| **FileWriter** | Write data to binary files | ZMQ PULL | File |
| **MonitorROOT** | Display histograms via web browser | ZMQ PULL | HTTP |

### Typical Pipeline

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Emulator 0  │────▶│             │────▶│ FileWriter  │
│ (port 5555) │     │ SimpleMerger│     │             │
└─────────────┘     │ (port 5560) │     └─────────────┘
                    │             │
┌─────────────┐     │             │     ┌─────────────┐
│ Emulator 1  │────▶│             │────▶│ MonitorROOT │
│ (port 5556) │     │             │     │ (HTTP 8080) │
└─────────────┘     └─────────────┘     └─────────────┘
```

## Building

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

The example executables will be in `build/examples/`:
- `delila_emulator`
- `delila_merger`
- `delila_writer`
- `delila_monitor` (if ROOT is available)

## Quick Start

### Step 1: Start the FileWriter

The FileWriter must be started first because it needs to be ready to receive data.

```bash
# Terminal 1
./delila_writer -i tcp://localhost:5560 -d ./data -p run_
```

### Step 2: Start the MonitorROOT (optional)

```bash
# Terminal 2
./delila_monitor -i tcp://localhost:5560 -p 8080 --2d
```

Open http://localhost:8080 in a web browser.

### Step 3: Start the SimpleMerger

The merger connects to downstream (FileWriter, Monitor) and waits for upstream sources.

```bash
# Terminal 3
./delila_merger -i tcp://localhost:5555 -i tcp://localhost:5556 -o tcp://*:5560
```

### Step 4: Start the Emulators

Start one or more emulators to generate data.

```bash
# Terminal 4
./delila_emulator -o tcp://*:5555 -m 0 -r 1000

# Terminal 5
./delila_emulator -o tcp://*:5556 -m 1 -r 1000
```

### Step 5: Stop the Pipeline

Press `Ctrl+C` in each terminal to stop components.
Stop order: Emulators → Merger → Writer/Monitor

## Component Details

### Emulator

Generates synthetic event data for testing.

```bash
./delila_emulator [options]

Options:
  -o, --output <address>   ZMQ output address (default: tcp://*:5555)
  -m, --module <number>    Module number 0-255 (default: 0)
  -c, --channels <number>  Number of channels 1-64 (default: 16)
  -r, --rate <events/sec>  Event generation rate (default: 1000)
  -e, --energy <min,max>   Energy range (default: 0,16383)
  --full                   Use full EventData mode (default: Minimal)
  --waveform <size>        Waveform samples (Full mode only)
  --seed <value>           Random seed for reproducibility
```

**Data Modes:**
- **Minimal**: 22 bytes per event, no waveform (high throughput)
- **Full**: Variable size, includes waveform data

### SimpleMerger

Merges multiple input streams into one output stream.

```bash
./delila_merger [options]

Options:
  -i, --input <address>    ZMQ input address (can specify multiple)
  -o, --output <address>   ZMQ output address (default: tcp://*:5560)
```

**Note:** The merger does NOT perform time-sorting. Events are forwarded in arrival order.

### FileWriter

Writes received data to binary files.

```bash
./delila_writer [options]

Options:
  -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)
  -d, --dir <path>         Output directory (default: current directory)
  -p, --prefix <string>    File prefix (default: run_)
```

**Output format:** Binary files named `<prefix><run_number>.dat`

### MonitorROOT

Displays real-time histograms via web browser (requires ROOT).

```bash
./delila_monitor [options]

Options:
  -i, --input <address>    ZMQ input address (default: tcp://localhost:5560)
  -p, --port <number>      HTTP server port (default: 8080)
  --no-energy              Disable energy histogram
  --no-channel             Disable channel histogram
  --timing                 Enable timing histogram
  --2d                     Enable 2D histograms
  --waveform <mod,ch>      Enable waveform display
```

**Available histograms:**
- Energy distribution (all channels)
- Channel distribution
- Module distribution
- Time difference between events
- Energy vs Channel (2D)
- Energy vs Module (2D)
- Waveform display (TGraph)

## Network Configuration

### ZMQ Address Format

```
tcp://<host>:<port>

Examples:
  tcp://*:5555          # Bind to all interfaces, port 5555
  tcp://localhost:5555  # Connect to localhost, port 5555
  tcp://192.168.1.10:5555  # Connect to specific IP
```

### Distributed Deployment

Components can run on different machines:

```
Machine A (Digitizers):
  ./delila_emulator -o tcp://*:5555 -m 0
  ./delila_emulator -o tcp://*:5556 -m 1

Machine B (Processing):
  ./delila_merger -i tcp://machineA:5555 -i tcp://machineA:5556 -o tcp://*:5560

Machine C (Storage):
  ./delila_writer -i tcp://machineB:5560 -d /data/experiment

Machine D (Monitoring):
  ./delila_monitor -i tcp://machineB:5560 -p 8080
```

## Troubleshooting

### "Failed to initialize"

- Check that addresses are correct
- Ensure no other process is using the same port
- Verify network connectivity between machines

### "Failed to arm" or "Failed to connect"

- Start downstream components first (FileWriter, Monitor)
- Then start the merger
- Finally start upstream sources (Emulator, DigitizerSource)

### No data in FileWriter/Monitor

- Check that all components are in "Running" state
- Verify network connectivity
- Check ZMQ addresses match (bind vs connect)

### MonitorROOT not available

- Rebuild DELILA2 with ROOT installed
- Ensure ROOTSYS environment variable is set
- Check that ROOT includes HttpServer component

## Advanced Topics

### Custom Run Numbers

Each component's `Start()` method accepts a run number.
In the example executables, run number is fixed to 1.
For production use, implement a run control system.

### Multiple Outputs from Merger

SimpleMerger currently supports one output.
For multiple outputs (e.g., FileWriter + Monitor), you can:
1. Run two separate pipelines
2. Use a secondary merger as a "splitter"

### Integration with Real Digitizers

Replace Emulator with DigitizerSource:

```bash
# Instead of emulator
./delila_digitizer_source -o tcp://*:5555 --config digitizer.json
```

(Note: DigitizerSource requires CAEN FELib and digitizer hardware)

## Support

For issues and feature requests, please use the project's issue tracker.
