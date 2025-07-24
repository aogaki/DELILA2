# Timestamp Decoding Fix for PSD1 Extra Options

## Issue Description
The original implementation incorrectly assumed all extra options had the same format (option 0b010) and always applied fine timestamp corrections. This was incorrect because:

1. **Extended timestamps** are present in extra options **000**, **001**, and **010**
2. **Fine timestamps** are only present in extra option **010**
3. **Event flags** are only present in extra option **010**

## PSD1 Extra Option Formats

### Option 0b000 (0) - Extended Timestamp Only
- **Bits [31:16]**: Extended timestamp (16 bits)
- **Bits [15:0]**: Reserved/unused
- **No fine timestamp**
- **No event flags**

### Option 0b001 (1) - Extended Timestamp Only
- **Bits [31:16]**: Extended timestamp (16 bits)
- **Bits [15:0]**: Reserved/unused
- **No fine timestamp**
- **No event flags**

### Option 0b010 (2) - Extended Timestamp + Flags + Fine Timestamp
- **Bits [31:16]**: Extended timestamp (16 bits)
- **Bits [15:10]**: Event flags (6 bits)
- **Bits [9:0]**: Fine timestamp (10 bits)
- **Full format with all components**

### Options 0b011-0b111 (3-7) - Other Formats
- **Implementation-specific formats**
- **Currently treated as extended timestamp only**
- **May require future implementation**

## Implementation Changes

### 1. Updated `DecodeExtrasWord` Method
```cpp
void DecodeExtrasWord(uint32_t extrasWord, uint8_t extraOption, EventData& eventData,
                     uint16_t& extendedTime, uint16_t& fineTimeStamp);
```

**Key Changes:**
- Added `extraOption` parameter to determine decoding format
- Returns `extendedTime` and `fineTimeStamp` by reference
- Processes event flags only for option 010

### 2. Updated `DecodeEventTimestamp` Method
```cpp
// Add fine time correction only for option 010
double fineTimeNs = 0.0;
if (dualChInfo.extraOption == PSD1Constants::ExtraFormats::kExtendedFlagsFineTT) {
    fineTimeNs = static_cast<double>(fineTimeStamp) * fFineTimeMultiplier;
}
```

**Key Changes:**
- Fine timestamp correction only applied for option 010
- Proper handling of all extra option formats
- Enhanced debug logging with extra option information

### 3. Added Constants for Extra Options
```cpp
namespace ExtraFormats {
    constexpr uint8_t kExtendedTimestampOnly = 0;   // Options 0b000 and 0b001
    constexpr uint8_t kExtendedTimestampOnly1 = 1;
    constexpr uint8_t kExtendedFlagsFineTT = 2;     // Option 0b010
}
```

## Timestamp Calculation Logic

### For Options 000 and 001:
```cpp
finalTimestamp = (extendedTime << 31 + triggerTimeTag) * timeStep
// No fine time correction
```

### For Option 010:
```cpp
finalTimestamp = (extendedTime << 31 + triggerTimeTag) * timeStep + fineTimeCorrection
fineTimeCorrection = (fineTimeStamp / 1024.0) * timeStep
```

## Validation and Testing

### Debug Output Enhanced
```
Timestamp calc: trigger=12345, extended=67, combined=891011, fine=123, final=891011.456 ns, extraOption=2
```

### Error Handling
- Validates extra option values
- Provides warnings for unknown options
- Graceful fallback for undefined formats

## Benefits

1. **Correct Timestamp Calculation**: Proper handling of different extra option formats
2. **Improved Accuracy**: Fine timestamps only applied when available
3. **Better Debugging**: Enhanced logging with extra option information
4. **Future-Proof**: Extensible design for additional extra option formats
5. **Maintains Compatibility**: Existing option 010 behavior preserved

## Impact on Existing Code

- **Backwards Compatible**: Option 010 behavior unchanged
- **Improved Accuracy**: Options 000 and 001 now decoded correctly
- **No Breaking Changes**: External API remains the same
- **Enhanced Debugging**: Better diagnostic information available

## Files Modified

1. **`src/Dig1Decoder.cpp`**: Updated timestamp decoding logic
2. **`include/Dig1Decoder.hpp`**: Updated method signatures
3. **`include/PSD1Constants.hpp`**: Added extra option constants

This fix ensures that the Dig1Decoder correctly handles all PSD1 extra option formats, providing accurate timestamp calculations regardless of the configured extra option setting.