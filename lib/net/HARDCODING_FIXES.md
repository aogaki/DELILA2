# Hardcoding Issues and Fixes

## Current Hardcoding Analysis

### ✅ Acceptable Hardcoding (Protocol Required)
- Magic numbers, protocol versions, header sizes
- These are part of the binary format specification

### ⚠️ Issues to Address

#### 1. Performance Limits Should Be Configurable
**Current:**
```cpp
constexpr size_t MAX_EVENTS_PER_MESSAGE = 10000;
constexpr size_t MIN_MESSAGE_SIZE = 100 * 1024;
constexpr int DEFAULT_COMPRESSION_LEVEL = 1;
```

**Proposed Fix:**
```cpp
// Keep defaults but allow runtime configuration
struct SerializationConfig {
    size_t max_events_per_message = 10000;
    size_t min_message_size = 100 * 1024;
    int default_compression_level = 1;
    
    // Add validation
    bool validate() const {
        return max_events_per_message > 0 && 
               min_message_size > 0 &&
               default_compression_level >= 1 && default_compression_level <= 12;
    }
};
```

#### 2. Test Date Ranges Need Updating
**Current:**
```cpp
EXPECT_GE(unix_timestamp, 1577836800000000000ULL); // After 2020
EXPECT_LE(unix_timestamp, 1893456000000000000ULL); // Before 2030
```

**Proposed Fix:**
```cpp
// Use dynamic date ranges
auto now = std::chrono::system_clock::now();
auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
auto year_ago = now_ns - (365LL * 24 * 60 * 60 * 1000000000LL);
auto year_ahead = now_ns + (365LL * 24 * 60 * 60 * 1000000000LL);

EXPECT_GE(unix_timestamp, year_ago);
EXPECT_LE(unix_timestamp, year_ahead);
```

#### 3. Build System Flexibility
**Current:**
```cmake
set(CMAKE_BUILD_TYPE Debug)  # Hardcoded default
```

**Proposed Fix:**
```cmake
# More flexible build type handling
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
```

## Implementation Priority

### High Priority
1. **Fix test date ranges** - Will break after 2030
2. **Make performance limits configurable** - Important for production use

### Medium Priority  
3. **Build system flexibility** - Minor improvement

### Low Priority
4. **Protocol constants** - These should remain hardcoded as they define the binary format

## Status
- **Analysis Complete**: ✅
- **Fixes Identified**: ✅
- **Implementation**: Pending user approval