# DELILA2 Phase 2: Core Messaging Implementation Plan

## Overview

This document outlines the detailed implementation plan for Phase 2 of the DELILA2 networking library, focusing on core messaging functionality with ZeroMQ integration, connection management, and message patterns.

## Objectives

- **Primary Goal**: Implement ZeroMQ-based transport layer with automatic TCP/IPC selection
- **Connection Management**: Robust connection handling with automatic recovery
- **Message Patterns**: Support for PUB/SUB, PUSH/PULL, REQ/REP, and DEALER/ROUTER
- **Quality Target**: 100% test coverage with comprehensive unit and integration tests
- **Performance Target**: Maintain serialization performance (6.5 GB/s) through efficient transport

## Prerequisites

### Completed Components (Phase 1)
- ✅ BinarySerializer with LZ4 compression
- ✅ EventData serialization/deserialization
- ✅ Error handling with Result<T> pattern
- ✅ Test data generation utilities

### Required Dependencies
- **ZeroMQ** >= 4.3.0 (messaging library)
- **cppzmq** >= 4.7.0 (C++ bindings for ZeroMQ)
- **EventData.hpp** (from DELILA digitizer library)
- **GoogleTest** (for unit testing)
- **Google Benchmark** (for performance testing)

## Implementation Schedule

**Estimated Duration**: 10-12 days

### Week 1: Transport Foundation (Days 1-6)
- Days 1-2: Connection class and transport selection
- Days 3-4: Socket configuration and error handling
- Days 5-6: Message pattern implementations

### Week 2: Advanced Features (Days 7-12)
- Days 7-8: Connection recovery and monitoring
- Days 9-10: Integration testing
- Days 11-12: Performance optimization and benchmarking

## Detailed Implementation Phases

### Phase 2.1: ZeroMQ Wrapper Classes (Days 1-2)

#### Tasks:
1. **Create Connection Base Class**
   - Abstract interface for all connection types
   - Common configuration parameters
   - Thread-safety considerations

2. **Implement Transport Selection Logic**
   - Automatic IPC selection for localhost/127.0.0.1
   - TCP for remote hosts
   - Manual override capability

3. **Socket Configuration**
   - High Water Mark (HWM) settings
   - Timeout configuration (5 second default)
   - TCP keepalive settings
   - Linger and reconnect intervals

#### Test Requirements:
- Unit tests for transport selection logic
- Configuration validation tests
- Thread safety tests
- Mock ZeroMQ socket tests

#### Success Criteria:
- [ ] Connection class compiles without errors
- [ ] Transport selection works correctly for all address types
- [ ] Socket options are properly applied
- [ ] All unit tests pass

### Phase 2.2: Connection Management (Days 3-4)

#### Tasks:
1. **Implement Connection Lifecycle**
   - Connection establishment
   - Graceful disconnection
   - Resource cleanup

2. **Error Recovery Mechanisms**
   - Automatic reconnection logic
   - Exponential backoff (100ms to 30s)
   - Connection state tracking
   - Error callback integration

3. **Interface Binding Support**
   - Bind to specific network interfaces
   - IP address resolution
   - Multi-interface support

#### Test Requirements:
- Connection establishment/teardown tests
- Network failure simulation tests
- Reconnection behavior tests
- Interface binding tests

#### Success Criteria:
- [ ] Connections establish and close cleanly
- [ ] Automatic reconnection works with backoff
- [ ] Interface binding functions correctly
- [ ] Error states are properly reported

### Phase 2.3: Message Pattern Implementation (Days 5-6)

#### Tasks:
1. **PUB/SUB Pattern**
   - Publisher class implementation
   - Subscriber class with topic filtering
   - Binary data distribution support

2. **PUSH/PULL Pattern**
   - Pusher class for work distribution
   - Puller class for work collection
   - Load balancing verification

3. **REQ/REP Pattern**
   - Request class with timeout handling
   - Reply class for synchronous responses
   - Correlation ID support

4. **DEALER/ROUTER Pattern**
   - Dealer class for async requests
   - Router class for request routing
   - Identity management

#### Test Requirements:
- Pattern-specific unit tests
- Message delivery verification
- Load balancing tests
- Timeout handling tests

#### Success Criteria:
- [ ] All patterns send/receive messages correctly
- [ ] Load balancing works for PUSH/PULL
- [ ] Topic filtering works for PUB/SUB
- [ ] Async patterns handle multiple connections

### Phase 2.4: Message Serialization Integration (Days 7-8)

#### Tasks:
1. **Binary Message Wrapper**
   - Integrate BinarySerializer with transport
   - Zero-copy optimization where possible
   - Message framing for multi-part messages

2. **Control Message Support**
   - Protocol Buffer integration
   - JSON message handling
   - Message type identification

3. **Performance Optimization**
   - Buffer reuse strategies
   - Minimize memory allocations
   - Batch message handling

#### Test Requirements:
- Serialization integration tests
- Multi-part message tests
- Performance benchmarks
- Memory usage profiling

#### Success Criteria:
- [ ] Binary events serialize/deserialize through transport
- [ ] No performance degradation from Phase 1
- [ ] Memory usage remains optimal
- [ ] All message types handled correctly

### Phase 2.5: Connection Recovery & Monitoring (Days 9-10)

#### Tasks:
1. **Advanced Recovery Features**
   - Connection health monitoring
   - Heartbeat mechanism
   - Dead peer detection
   - Graceful degradation

2. **Statistics Collection**
   - Message counters (sent/received)
   - Throughput calculation
   - Error rate tracking
   - Connection uptime

3. **Sequence Number Integration**
   - Gap detection at transport level
   - Gap reporting callbacks
   - Statistics for gap frequency

#### Test Requirements:
- Long-running stability tests
- Network partition tests
- Statistics accuracy tests
- Recovery scenario tests

#### Success Criteria:
- [ ] Connections recover from all failure modes
- [ ] Statistics accurately track performance
- [ ] Gap detection works reliably
- [ ] No resource leaks over time

### Phase 2.6: Integration Testing (Days 11-12)

#### Tasks:
1. **Multi-Module Test Scenarios**
   - Data fetcher → merger flow
   - Merger → sink broadcast
   - Controller coordination
   - Full system simulation

2. **Failure Scenarios**
   - Module crash simulation
   - Network partition handling
   - Buffer overflow behavior
   - Timeout scenarios

3. **Performance Validation**
   - Throughput benchmarks with real data
   - Latency measurements
   - Scalability testing (multiple connections)
   - Resource usage profiling

#### Test Requirements:
- End-to-end integration tests
- Stress tests with high message rates
- Failure injection tests
- Performance regression tests

#### Success Criteria:
- [ ] All integration tests pass
- [ ] Performance meets targets (100 MB/s typical)
- [ ] System recovers from failures
- [ ] No memory leaks or resource issues

## Risk Assessment

### High Risk Items

1. **ZeroMQ Version Compatibility**
   - **Risk**: API differences between ZeroMQ versions
   - **Mitigation**: Test with minimum supported version, use stable API subset
   - **Impact**: May limit feature availability

2. **Thread Safety Complexity**
   - **Risk**: ZeroMQ sockets are not thread-safe
   - **Mitigation**: Clear ownership model, thread-local storage
   - **Impact**: Requires careful design consideration

3. **Performance Impact**
   - **Risk**: Transport layer adds latency/overhead
   - **Mitigation**: Zero-copy where possible, benchmark continuously
   - **Impact**: May not maintain full 6.5 GB/s throughput

### Medium Risk Items

1. **Platform Differences**
   - **Risk**: Network behavior varies between Linux/macOS
   - **Mitigation**: Abstract platform-specific code, test on both
   - **Impact**: Additional testing complexity

2. **Connection Recovery Edge Cases**
   - **Risk**: Complex failure modes difficult to handle
   - **Mitigation**: Extensive failure testing, simple recovery logic
   - **Impact**: Some edge cases may require manual intervention

## Success Criteria

### Core Functionality
- [ ] All message patterns implemented and tested
- [ ] Automatic transport selection works correctly
- [ ] Connection recovery handles common failures
- [ ] Performance meets minimum targets

### Code Quality
- [ ] 100% unit test coverage for new code
- [ ] All integration tests passing
- [ ] No memory leaks detected
- [ ] Clean static analysis results

### Performance Metrics
- [ ] Message throughput ≥ 100 MB/s (typical)
- [ ] Connection establishment < 100ms
- [ ] Reconnection time < 5 seconds
- [ ] Memory overhead < 10% of message size

### Documentation
- [ ] API documentation complete
- [ ] Integration guide written
- [ ] Performance tuning guide
- [ ] Example code for each pattern

## Testing Strategy

### Unit Test Structure
```
tests/unit/transport/
├── test_connection.cpp
├── test_transport_selection.cpp
├── test_socket_config.cpp
├── test_pub_sub.cpp
├── test_push_pull.cpp
├── test_req_rep.cpp
├── test_dealer_router.cpp
├── test_recovery.cpp
└── test_statistics.cpp
```

### Integration Test Scenarios
1. **Basic Communication**: Single sender/receiver pairs
2. **Multiple Connections**: Many-to-one and one-to-many
3. **Failure Recovery**: Network interruption handling
4. **Performance**: Sustained throughput testing
5. **Mixed Patterns**: Multiple patterns in single system

### Benchmark Suite
- Connection establishment time
- Message round-trip latency
- Throughput vs message size
- Scalability with connection count
- Recovery time measurements

## Deliverables

### Code Deliverables
1. Connection class hierarchy
2. Pattern-specific implementations
3. Transport selection logic
4. Recovery and monitoring systems
5. Integration with BinarySerializer

### Test Deliverables
1. Comprehensive unit test suite
2. Integration test scenarios
3. Performance benchmarks
4. Failure injection tests

### Documentation Deliverables
1. API reference documentation
2. Integration guide with examples
3. Performance tuning guide
4. Troubleshooting guide

## Next Steps

After Phase 2 completion:
- **Phase 3**: Advanced features (priority queuing, advanced monitoring)
- **Phase 4**: Full system integration and production hardening

## Notes

- Maintain backward compatibility with Phase 1 serialization
- Keep thread safety model simple and well-documented
- Focus on reliability over complex features
- Ensure graceful degradation under load