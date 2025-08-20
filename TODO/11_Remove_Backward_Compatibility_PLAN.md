# Refactoring Plan: Remove Backward Compatibility from lib/net

## Overview
Remove all backward compatibility code from DELILA2's lib/net to simplify the codebase and enforce the new DataProcessor-based architecture. This involves removing deprecated Serializer wrapper and updating all dependent code.

## Goals
1. **Simplify Architecture**: Remove deprecated Serializer wrapper class
2. **Enforce New API**: Force migration to DataProcessor + ZMQTransport byte-based approach
3. **Clean Codebase**: Remove deprecated methods from ZMQTransport
4. **Update Examples**: Migrate all examples to use new API patterns
5. **Update Tests**: Ensure all tests use new API patterns

## Strategy

### Phase 1: Analysis and Documentation
- Document current usage of deprecated APIs
- Identify all files that need modification
- Create migration patterns for common use cases

### Phase 2: Core Library Refactoring
1. Remove Serializer.hpp and Serializer.cpp completely
2. Remove deprecated methods from ZMQTransport:
   - `Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>&)`
   - `Receive()` → `std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t>`
   - `SendMinimal(const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>&)`
   - `ReceiveMinimal()` → `std::pair<std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>, uint64_t>`
   - `PeekDataType()`
   - `ReceiveAny()`
3. Remove Serializer include from ZMQTransport.hpp
4. Remove all temp_serializer usage from ZMQTransport.cpp
5. Update CMakeLists.txt to remove Serializer compilation

### Phase 3: Examples Migration
- Update all examples to use DataProcessor + SendBytes/ReceiveBytes pattern
- Remove deprecated API usage
- Add clear documentation showing new patterns

### Phase 4: Tests Migration  
- Update integration tests to use new API only
- Remove backward compatibility tests
- Update unit tests for ZMQTransport to only test byte-based methods

### Phase 5: Documentation Updates
- Update all README and MANUAL files
- Remove references to deprecated Serializer class
- Update code examples in documentation

## Migration Pattern

### Old Pattern (to be removed):
```cpp
ZMQTransport transport;
// ... configure transport ...
auto events = CreateEvents();
transport.Send(events);  // DEPRECATED

auto [received_events, seq] = transport.Receive();  // DEPRECATED
```

### New Pattern (enforced):
```cpp
ZMQTransport transport;
DataProcessor processor;
// ... configure transport ...

// Sending
auto events = CreateEvents();
auto bytes = processor.Process(events, sequence_number);
transport.SendBytes(bytes);

// Receiving  
auto bytes = transport.ReceiveBytes();
auto [received_events, seq] = processor.Decode(bytes);
```

## Risk Assessment

### Low Risk
- Examples and documentation updates
- Test updates
- Removing unused methods

### Medium Risk  
- ZMQTransport refactoring (well-tested new methods exist)
- Integration test updates (new patterns are proven)

### High Risk
- Complete Serializer removal (many dependencies)
- Ensuring no external code depends on removed APIs

## Rollback Plan
- Keep git history for easy rollback
- Comprehensive testing before finalizing changes
- Document all breaking changes clearly

## Success Criteria
1. ✅ All deprecated methods removed from ZMQTransport
2. ✅ Serializer.hpp and Serializer.cpp completely removed
3. ✅ All examples use DataProcessor + byte-based transport
4. ✅ All tests pass with new API only
5. ✅ Documentation updated and consistent
6. ✅ No compilation warnings or errors
7. ✅ Performance maintained or improved

## Timeline
- **Analysis**: 30 minutes
- **Core refactoring**: 2-3 hours  
- **Examples migration**: 1 hour
- **Tests migration**: 1 hour
- **Documentation updates**: 30 minutes
- **Testing and validation**: 1 hour

**Total Estimated Time**: 5-6 hours

## Breaking Changes Notice
This refactoring introduces **breaking changes**:
- `Serializer` class completely removed
- Deprecated `ZMQTransport` methods removed
- All user code must migrate to `DataProcessor` + byte-based transport

This is an intentional breaking change to simplify and modernize the architecture.