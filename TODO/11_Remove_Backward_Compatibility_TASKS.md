# Detailed Tasks: Remove Backward Compatibility from lib/net

## Task Checklist

### Phase 1: Analysis and Preparation ✅
- [x] Document current Serializer usage across codebase
- [x] Identify files requiring modification
- [x] Create refactoring plan
- [x] Create detailed task list

### Phase 2: Core Library Refactoring

#### 2.1 Remove Serializer Files
- [ ] Delete `lib/net/include/Serializer.hpp`
- [ ] Delete `lib/net/src/Serializer.cpp` 
- [ ] Update `lib/net/CMakeLists.txt` to remove Serializer compilation

#### 2.2 Refactor ZMQTransport.hpp
- [ ] Remove `#include "Serializer.hpp"` (line 14)
- [ ] Remove deprecated method declarations:
  - [ ] `Send(const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>&)`
  - [ ] `Receive()` returning EventData pair
  - [ ] `SendMinimal(const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>&)`
  - [ ] `ReceiveMinimal()` returning MinimalEventData pair
  - [ ] `PeekDataType()`
  - [ ] `ReceiveAny()`
  - [ ] `WaitForCommand()` and related command methods
  - [ ] `SendStatus()`, `ReceiveStatus()`, `SendCommand()`, `SendCommandResponse()` methods
- [ ] Clean up deprecated method comments and [[deprecated]] attributes
- [ ] Remove DataType enum if no longer needed

#### 2.3 Refactor ZMQTransport.cpp
- [ ] Remove all `Serializer temp_serializer;` instances (lines ~380, 459, 488, 566, 671, 678, 688)
- [ ] Remove deprecated method implementations:
  - [ ] `Send()` method implementation
  - [ ] `Receive()` method implementation  
  - [ ] `SendMinimal()` method implementation
  - [ ] `ReceiveMinimal()` method implementation
  - [ ] `PeekDataType()` method implementation
  - [ ] `ReceiveAny()` method implementation
- [ ] Remove command/status handling methods if they use Serializer
- [ ] Clean up includes (remove Serializer.hpp include)
- [ ] Remove sequence number tracking for deprecated methods
- [ ] Simplify class members - remove deprecated-method-related member variables

### Phase 3: Examples Migration

#### 3.1 Update examples/data_command_example.cpp
- [ ] Replace `#include "Serializer.hpp"` with `#include "DataProcessor.hpp"`
- [ ] Replace `Serializer serializer_;` with `DataProcessor processor_;`
- [ ] Update serialization calls: `serializer_.Encode()` → `processor_.Process()`
- [ ] Update deserialization calls: `serializer_.Decode()` → `processor_.Decode()`
- [ ] Update `SendBytes()` and `ReceiveBytes()` usage patterns
- [ ] Test example compilation and execution

#### 3.2 Update examples/minimal_event_data_example.cpp  
- [ ] Replace Serializer includes with DataProcessor
- [ ] Update all serialization/deserialization code
- [ ] Remove deprecated `SendMinimal()` and `ReceiveMinimal()` usage
- [ ] Update to use `SendBytes()` and `ReceiveBytes()` exclusively
- [ ] Update comments and documentation in example

#### 3.3 Update examples/separated_serialization.cpp
- [ ] Replace Serializer usage with DataProcessor
- [ ] Update old API vs new API comparison sections
- [ ] Remove old API demonstration code
- [ ] Focus example on proper DataProcessor + ZMQTransport usage

#### 3.4 Update examples/memory_pool_example.cpp
- [ ] Replace Serializer includes and usage
- [ ] Update to use DataProcessor for serialization
- [ ] Ensure memory pool patterns still work with new API

### Phase 4: Tests Migration

#### 4.1 Update tests/integration/test_api_interoperability.cpp
- [ ] Remove backward compatibility tests entirely OR
- [ ] Convert to test DataProcessor vs direct byte manipulation
- [ ] Remove Serializer includes and usage
- [ ] Update test helper functions to use DataProcessor
- [ ] Simplify test cases to focus on byte-transport reliability
- [ ] Remove "Old API" vs "New API" concepts - only one API now

#### 4.2 Update other test files
- [ ] Search for any remaining Serializer usage in test files
- [ ] Update test helper functions (`test_helpers.hpp`)
- [ ] Ensure all ZMQTransport tests only use byte-based methods
- [ ] Update benchmarks to use DataProcessor + byte transport

### Phase 5: Documentation Updates

#### 5.1 Update lib/net/README.md
- [ ] Remove Serializer class documentation (around line 57-97)
- [ ] Update examples to show DataProcessor usage
- [ ] Remove references to deprecated methods
- [ ] Simplify API documentation to focus on byte-based transport

#### 5.2 Update lib/net/MANUAL.md
- [ ] Remove Serializer usage examples (around line 85-97)
- [ ] Update all code examples to use DataProcessor
- [ ] Update use case examples to show proper patterns
- [ ] Remove deprecated method references

#### 5.3 Update lib/net/README_JP.md
- [ ] Apply same changes as English README
- [ ] Update Japanese examples and documentation

### Phase 6: Build System Updates

#### 6.1 Update CMakeLists.txt files
- [ ] Remove Serializer.cpp from lib/net/CMakeLists.txt
- [ ] Update any target dependencies
- [ ] Verify examples still compile correctly
- [ ] Update test targets if needed

#### 6.2 Update includes and exports
- [ ] Check if any export/install commands reference Serializer.hpp
- [ ] Update package config files if needed

### Phase 7: Testing and Validation

#### 7.1 Compilation Testing  
- [ ] Clean build from scratch
- [ ] Verify no compilation errors
- [ ] Verify no missing symbol errors
- [ ] Check examples compile successfully

#### 7.2 Runtime Testing
- [ ] Run all unit tests - ensure they pass
- [ ] Run all integration tests - ensure they pass  
- [ ] Run example programs - ensure they work
- [ ] Test basic send/receive functionality

#### 7.3 Performance Testing
- [ ] Run existing benchmarks
- [ ] Verify performance is maintained or improved
- [ ] Check memory usage patterns

### Phase 8: Final Cleanup

#### 8.1 Code Review
- [ ] Review all changed files for consistency
- [ ] Ensure no deprecated patterns remain
- [ ] Verify error handling is consistent
- [ ] Check for any missed TODO comments

#### 8.2 Documentation Review
- [ ] Verify all examples work as documented
- [ ] Check that migration patterns are clear
- [ ] Ensure no broken references remain

## Files to Modify

### Core Library Files
- `lib/net/include/Serializer.hpp` (DELETE)
- `lib/net/src/Serializer.cpp` (DELETE)
- `lib/net/include/ZMQTransport.hpp` (MODIFY - remove deprecated methods)
- `lib/net/src/ZMQTransport.cpp` (MODIFY - remove implementations)
- `lib/net/CMakeLists.txt` (MODIFY - remove Serializer compilation)

### Example Files  
- `examples/data_command_example.cpp` (MODIFY)
- `examples/minimal_event_data_example.cpp` (MODIFY)  
- `examples/separated_serialization.cpp` (MODIFY)
- `examples/memory_pool_example.cpp` (MODIFY)

### Test Files
- `tests/integration/test_api_interoperability.cpp` (MODIFY heavily)
- `tests/unit/net/test_helpers.hpp` (CHECK and possibly MODIFY)
- Any other test files with Serializer usage (CHECK)

### Documentation Files
- `lib/net/README.md` (MODIFY)
- `lib/net/MANUAL.md` (MODIFY) 
- `lib/net/README_JP.md` (MODIFY)

## Estimated Effort Per Task
- **File deletion**: 5 minutes
- **Header refactoring**: 30 minutes  
- **Source file refactoring**: 60 minutes
- **Example migration**: 15 minutes each
- **Test migration**: 30-45 minutes each
- **Documentation updates**: 15 minutes each
- **Build system updates**: 15 minutes
- **Testing and validation**: 60 minutes

**Total Estimated Time**: 5-6 hours

## Risk Mitigation
- Make frequent git commits for easy rollback
- Test compilation after each major change
- Keep DataProcessor functionality intact
- Preserve all working byte-based transport functionality