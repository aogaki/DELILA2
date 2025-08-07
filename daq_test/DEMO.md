# DELILA2 Source/Sink Demo

## Quick Test

Run the automated test script:
```bash
cd daq_test
./test_source_sink.sh
```

## Manual Testing

### Terminal 1 - Start Source
```bash
cd daq_test
source /opt/ROOT/bin/thisroot.sh
./build/bin/delila_source PSD2.conf "tcp://*:5555"
```

You should see:
- Digitizer initialization
- "Transport connected, starting acquisition..."
- Real-time statistics showing event rate

### Terminal 2 - Start Sink
```bash
cd daq_test  
source /opt/ROOT/bin/thisroot.sh
./build/bin/delila_sink --address tcp://localhost:5555
```

You should see:
- "Transport connected, waiting for data..."
- Real-time statistics showing received events
- Web monitor available at http://localhost:8080

### Testing Options

1. **Multiple Sinks**: Start multiple sink instances - they all receive the same data
2. **Compression**: Add `--compress` to both source and sink
3. **No Recording**: Add `--no-recorder` to sink to only monitor
4. **No Monitor**: Add `--no-monitor` to sink to only record data

### Stopping

Press 'q' in either terminal to stop that component gracefully.

## Expected Results

- Source publishes event data from digitizer via ZeroMQ
- Sink receives and processes the data
- Monitor provides real-time visualization (if enabled)
- Recorder saves data files (if enabled)
- Both components show matching event rates