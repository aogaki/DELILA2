/**
 * @file AMaxConstants.hpp
 * @brief Constants and register definitions for AMax (DELILA custom) firmware
 *
 * Register names and addresses from RegisterFile.json
 * Original spelling preserved for compatibility
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cctype>

namespace DELILA {
namespace Digitizer {
namespace AMaxConstants {

// ============================================================================
// Core Control Registers (0x0000 - 0x0007)
// ============================================================================

constexpr uint32_t REG_HOOOLD      = 0x0000;  ///< Hold-off time
constexpr uint32_t REG_M_LENGHT    = 0x0001;  ///< Moving average length (original spelling)
constexpr uint32_t REG_FT_DELAY    = 0x0002;  ///< Fast trigger delay
constexpr uint32_t REG_FT_HOLD     = 0x0003;  ///< Fast trigger hold
constexpr uint32_t REG_EN_SHIFT    = 0x0004;  ///< Enable shift
constexpr uint32_t REG_EN_OUT_H    = 0x0005;  ///< Enable output high
constexpr uint32_t REG_EN_SHIFT_IN = 0x0006;  ///< Enable shift input
constexpr uint32_t REG_GAIN        = 0x0007;  ///< Digital gain

// ============================================================================
// System Control Registers (0xE0020 - 0xE0023)
// ============================================================================

constexpr uint32_t REG_REG_EN      = 0xE0020;  ///< Register enable
constexpr uint32_t REG_REG_AMAX    = 0xE0021;  ///< AMax register control
constexpr uint32_t REG_VAL_EN      = 0xE0022;  ///< Value enable
constexpr uint32_t REG_VAL_AMAX    = 0xE0023;  ///< AMax value control

// ============================================================================
// Time Control Register
// ============================================================================

constexpr uint32_t REG_TIME_START  = 0x180003;  ///< Time start control

// ============================================================================
// CFD Parameters Component (base: 0x8000E)
// ============================================================================

constexpr uint32_t CFD_BASE_ADDR        = 0x8000E;
constexpr uint32_t REG_CFD_DELAY_IN     = 0x8000F;  ///< CFD delay input
constexpr uint32_t REG_CFD_FRACT_IN     = 0x80010;  ///< CFD fraction input
constexpr uint32_t REG_CFD_THRCFD       = 0x80011;  ///< CFD threshold
constexpr uint32_t REG_CFD_DELAY_TRIGGER = 0x80012; ///< CFD trigger delay
constexpr uint32_t REG_CFD_BASELINE_LEN = 0x80013;  ///< CFD baseline length

// ============================================================================
// Energy Parameters Component (base: 0xE000B)
// ============================================================================

constexpr uint32_t EN_ALL_BASE_ADDR     = 0xE000B;
constexpr uint32_t REG_EN_SUB1_LEN      = 0xE000C;  ///< Sub-window 1 length
constexpr uint32_t REG_EN_SUB1_DELAY    = 0xE000D;  ///< Sub-window 1 delay
constexpr uint32_t REG_EN_SUB2_DELAY    = 0xE000E;  ///< Sub-window 2 delay
constexpr uint32_t REG_EN_MULT          = 0xE000F;  ///< Multiplication factor

// ============================================================================
// AMax Input Parameters Component (base: 0xE001A)
// ============================================================================

constexpr uint32_t AMAX_IN_BASE_ADDR       = 0xE001A;
constexpr uint32_t REG_AMAX_SHAPUNG_LEN    = 0xE001B;  ///< Shaping length (original spelling)
constexpr uint32_t REG_AMAX_WINDOW_MAXIM   = 0xE001C;  ///< Maximum window
constexpr uint32_t REG_AMAX_DELAY_AM       = 0xE001D;  ///< Amplitude delay
constexpr uint32_t REG_AMAX_WINDOW         = 0xE001E;  ///< AMax window
constexpr uint32_t REG_AMAX_OFFSETT        = 0xE001F;  ///< Offset (original spelling)

// ============================================================================
// AMax Output Registers Component (base: 0xE0011) - READ ONLY
// ============================================================================

constexpr uint32_t AMAX_OUT_BASE_ADDR      = 0xE0011;
constexpr uint32_t REG_AMAX_NORM_MAXIM     = 0xE0012;  ///< Normalized maximum (RO)
constexpr uint32_t REG_AMAX_NORM_TEST      = 0xE0013;  ///< Normalized test value (RO)
constexpr uint32_t REG_AMAX_TEST_1         = 0xE0014;  ///< AMax test value 1 (RO)
constexpr uint32_t REG_AMAX_TEST_MAX       = 0xE0015;  ///< AMax test maximum (RO)
constexpr uint32_t REG_AMAX_TEST_MIN       = 0xE0016;  ///< AMax test minimum (RO)
constexpr uint32_t REG_AMAX_OUTPUT         = 0xE0017;  ///< AMax output value (RO)
constexpr uint32_t REG_AMAX_TEST_2         = 0xE0018;  ///< AMax test value 2 (RO)

// ============================================================================
// Baseline Input Component
// ============================================================================

constexpr uint32_t BASELINE_BASE_ADDR      = 0xE0008;
constexpr uint32_t REG_BASELINE_MINE_SLICED = 0xE0009;  ///< Baseline sliced value

// ============================================================================
// Oscilloscope Components
// ============================================================================

// Main Oscilloscope (Oscilloscope_finall)
constexpr uint32_t OSC_MAIN_BASE_ADDR           = 0x40000;
constexpr uint32_t REG_OSC_MAIN_READ_STATUS     = 0x80000;
constexpr uint32_t REG_OSC_MAIN_READ_POSITION   = 0x80001;
constexpr uint32_t REG_OSC_MAIN_TRIGGER_MODE    = 0x80002;
constexpr uint32_t REG_OSC_MAIN_PRETRIGGER      = 0x80003;
constexpr uint32_t REG_OSC_MAIN_TRIGGER_LEVEL   = 0x80004;
constexpr uint32_t REG_OSC_MAIN_ARM             = 0x80005;
constexpr uint32_t REG_OSC_MAIN_DECIMATOR       = 0x80006;

// Secondary Oscilloscope (Oscilloscope_0)
constexpr uint32_t OSC_SEC_BASE_ADDR            = 0xC0000;
constexpr uint32_t REG_OSC_SEC_READ_STATUS      = 0xE0000;
constexpr uint32_t REG_OSC_SEC_READ_POSITION    = 0xE0001;
constexpr uint32_t REG_OSC_SEC_TRIGGER_MODE     = 0xE0002;
constexpr uint32_t REG_OSC_SEC_PRETRIGGER       = 0xE0003;
constexpr uint32_t REG_OSC_SEC_TRIGGER_LEVEL    = 0xE0004;
constexpr uint32_t REG_OSC_SEC_ARM              = 0xE0005;
constexpr uint32_t REG_OSC_SEC_DECIMATOR        = 0xE0006;

// ============================================================================
// Spectrum Component
// ============================================================================

constexpr uint32_t SPECTRUM_BASE_ADDR       = 0x90000;
constexpr uint32_t REG_SPECTRUM_STATUS      = 0xA0000;
constexpr uint32_t REG_SPECTRUM_CONFIG      = 0xA0001;
constexpr uint32_t REG_SPECTRUM_LIMIT       = 0xA0002;
constexpr uint32_t REG_SPECTRUM_REBIN       = 0xA0003;
constexpr uint32_t REG_SPECTRUM_MIN         = 0xA0004;
constexpr uint32_t REG_SPECTRUM_MAX         = 0xA0005;

// ============================================================================
// Histogram2D Component
// ============================================================================

constexpr uint32_t HIST2D_BASE_ADDR         = 0x100000;
constexpr uint32_t REG_HIST2D_STATUS        = 0x180000;
constexpr uint32_t REG_HIST2D_CONFIG        = 0x180001;
constexpr uint32_t REG_HIST2D_LIMIT         = 0x180002;

// ============================================================================
// Register Name Map (for named access)
// ============================================================================

/**
 * @brief Map of register names to addresses
 * Uses original spelling from RegisterFile.json
 */
inline const std::unordered_map<std::string, uint32_t> REGISTER_MAP = {
    // Core registers
    {"hooold", REG_HOOOLD},
    {"M_lenght", REG_M_LENGHT},  // Original spelling preserved
    {"ft_delay", REG_FT_DELAY},
    {"ft_hold", REG_FT_HOLD},
    {"en_shift", REG_EN_SHIFT},
    {"en_out_h", REG_EN_OUT_H},
    {"en_shift_in", REG_EN_SHIFT_IN},
    {"GAIN", REG_GAIN},

    // System control
    {"reg_en", REG_REG_EN},
    {"reg_amax", REG_REG_AMAX},
    {"val_en", REG_VAL_EN},
    {"val_amax", REG_VAL_AMAX},
    {"time_start", REG_TIME_START},

    // CFD parameters (component: cfd_param)
    {"delay_in", REG_CFD_DELAY_IN},
    {"fract_in", REG_CFD_FRACT_IN},
    {"thrcfd", REG_CFD_THRCFD},
    {"delay_trigger", REG_CFD_DELAY_TRIGGER},
    {"baseline_len", REG_CFD_BASELINE_LEN},

    // Energy parameters (component: en_all)
    {"sub1_len", REG_EN_SUB1_LEN},
    {"sub1_delay", REG_EN_SUB1_DELAY},
    {"sub2_delay", REG_EN_SUB2_DELAY},
    {"mult", REG_EN_MULT},

    // AMax input parameters (component: amax_in)
    {"shapung_len", REG_AMAX_SHAPUNG_LEN},  // Original spelling preserved
    {"Window_MAXIM", REG_AMAX_WINDOW_MAXIM},
    {"DELAY_AM", REG_AMAX_DELAY_AM},
    {"AMAX_WINDOW", REG_AMAX_WINDOW},
    {"offsett", REG_AMAX_OFFSETT},  // Original spelling preserved

    // AMax output (read-only)
    {"norm_maxim", REG_AMAX_NORM_MAXIM},
    {"norm_test", REG_AMAX_NORM_TEST},
    {"amax_test_1", REG_AMAX_TEST_1},
    {"amax_test_max", REG_AMAX_TEST_MAX},
    {"amax_test_min", REG_AMAX_TEST_MIN},
    {"AMAX", REG_AMAX_OUTPUT},
    {"amax_test_2", REG_AMAX_TEST_2},

    // Baseline
    {"baseline_mine_sliced", REG_BASELINE_MINE_SLICED},
};

/**
 * @brief Get register address by name
 * @param name Register name (case-sensitive)
 * @return Register address, or throws if not found
 */
inline uint32_t GetRegisterAddress(const std::string& name) {
    auto it = REGISTER_MAP.find(name);
    if (it != REGISTER_MAP.end()) {
        return it->second;
    }

    // Try with component prefix removed (e.g., "cfd_param/delay_in" -> "delay_in")
    size_t slashPos = name.find('/');
    if (slashPos != std::string::npos) {
        std::string simpleName = name.substr(slashPos + 1);
        it = REGISTER_MAP.find(simpleName);
        if (it != REGISTER_MAP.end()) {
            return it->second;
        }
    }

    throw std::runtime_error("Unknown register name: " + name);
}

/**
 * @brief Check if a register address is read-only
 * @param address Register address
 * @return True if register is read-only
 */
inline bool IsReadOnlyRegister(uint32_t address) {
    return (address >= REG_AMAX_NORM_MAXIM && address <= REG_AMAX_TEST_2);
}

// ============================================================================
// Word Size and Basic Constants
// ============================================================================
constexpr size_t kWordSize = 8;  // 64-bit word size in bytes

// ============================================================================
// Header Constants
// ============================================================================
namespace Header
{
// Header type identification
constexpr uint64_t kTypeMask = 0xF;
constexpr int kTypeShift = 60;
constexpr uint64_t kTypeData = 0x2;

// Fail check
constexpr int kFailCheckShift = 56;
constexpr uint64_t kFailCheckMask = 0x1;

// Aggregate counter
constexpr int kAggregateCounterShift = 32;
constexpr uint64_t kAggregateCounterMask = 0xFFFF;

// Total size
constexpr uint64_t kTotalSizeMask = 0xFFFFFFFF;
}  // namespace Header

// ============================================================================
// Event Data Constants
// ============================================================================
namespace Event
{
// Channel
constexpr int kChannelShift = 56;
constexpr uint64_t kChannelMask = 0x7F;

// Timestamp
constexpr uint64_t kTimeStampMask = 0xFFFFFFFFFFFF;

// Flags
constexpr int kLastWordShift = 63;
constexpr int kWaveformFlagShift = 62;

// Flags - Low Priority
constexpr int kFlagsLowPriorityShift = 50;
constexpr uint64_t kFlagsLowPriorityMask = 0x7FF;

// Flags - High Priority
constexpr int kFlagsHighPriorityShift = 42;
constexpr uint64_t kFlagsHighPriorityMask = 0xFF;

// Energy Short
constexpr int kEnergyShortShift = 26;
constexpr uint64_t kEnergyShortMask = 0xFFFF;

// Fine Time
constexpr int kFineTimeShift = 16;
constexpr uint64_t kFineTimeMask = 0x3FF;
constexpr double kFineTimeScale = 1024.0;

// Energy (Long)
constexpr uint64_t kEnergyMask = 0xFFFF;
}  // namespace Event

// ============================================================================
// Waveform Constants
// ============================================================================
namespace Waveform
{
// Waveform header checks
constexpr int kWaveformCheck1Shift = 63;
constexpr int kWaveformCheck2Shift = 60;
constexpr uint64_t kWaveformCheck2Mask = 0x7;

// Time resolution
constexpr int kTimeResolutionShift = 44;
constexpr uint64_t kTimeResolutionMask = 0x3;

// Waveform words count
constexpr uint64_t kWaveformWordsMask = 0xFFF;

// Probe configuration
constexpr uint64_t kAnalogProbeMask = 0x3FFF;
constexpr uint64_t kDigitalProbeMask = 0x1;

// Probe type positions in header
constexpr int kDigitalProbe4TypeShift = 24;
constexpr int kDigitalProbe3TypeShift = 20;
constexpr int kDigitalProbe2TypeShift = 16;
constexpr int kDigitalProbe1TypeShift = 12;
constexpr int kAnalogProbe2TypeShift = 6;
constexpr int kAnalogProbe1TypeShift = 0;
constexpr uint64_t kProbeTypeMask = 0xF;
constexpr uint64_t kAnalogProbeTypeMask = 0x7;

// Analog probe configuration
constexpr int kAP1SignedShift = 3;
constexpr int kAP1MulFactorShift = 4;
constexpr int kAP2SignedShift = 9;
constexpr int kAP2MulFactorShift = 10;
constexpr uint64_t kAPMulFactorMask = 0x3;

// Waveform point decoding
constexpr int kAnalogProbe2Shift = 16;
constexpr int kDigitalProbe1Shift = 14;
constexpr int kDigitalProbe2Shift = 15;
constexpr int kDigitalProbe3Shift = 30;
constexpr int kDigitalProbe4Shift = 31;

// Sign extension for 14-bit analog probes
constexpr uint32_t kAnalogProbeSignBit = 0x2000;      // Bit 13
constexpr uint32_t kAnalogProbeSignExtend = 0xFFFFC000;  // Extend to 32-bit

// Sample extraction masks
constexpr uint64_t kSample1Mask = 0xFFFFFFFF;  // Lower 32 bits
constexpr uint64_t kSample2Shift = 32;         // Upper 32 bits shift
}  // namespace Waveform

// ============================================================================
// Start/Stop Signal Constants
// ============================================================================
namespace StartStop
{
// Start signal pattern
constexpr uint64_t kStartFirstWordType = 0x3;
constexpr uint64_t kStartFirstWordSubType = 0x0;
constexpr uint64_t kStartFirstWordExtra1 = 0x3;
constexpr uint64_t kStartFirstWordExtra2 = 0x4;
constexpr uint64_t kStartSecondWordType = 0x2;
constexpr uint64_t kStartThirdWordType = 0x1;
constexpr uint64_t kStartFourthWordType = 0x1;

// Stop signal pattern
constexpr uint64_t kStopFirstWordType = 0x3;
constexpr uint64_t kStopFirstWordSubType = 0x2;
constexpr uint64_t kStopSecondWordType = 0x0;
constexpr uint64_t kStopThirdWordType = 0x1;

// Common bit positions
constexpr int kSignalTypeShift = 60;
constexpr int kSignalSubTypeShift = 56;
constexpr int kSignalExtra1Shift = 32;
constexpr int kSignalExtra2Shift = 0;
constexpr uint64_t kSignalTypeMask = 0xF;
constexpr uint64_t kSignalExtra1Mask = 0xFF;
constexpr uint64_t kSignalExtra2Mask = 0xFFFFFFFF;

// Dead time in stop signal
constexpr uint64_t kDeadTimeMask = 0xFFFFFFFF;
constexpr int kDeadTimeScale = 8;  // ns
}  // namespace StartStop

// ============================================================================
// Validation Constants
// ============================================================================
namespace Validation
{
constexpr size_t kMinimumDataSize = 3 * kWordSize;   // Minimum for stop signal
constexpr size_t kStartSignalSize = 4 * kWordSize;   // Start signal size
constexpr size_t kStopSignalSize = 3 * kWordSize;    // Stop signal size
constexpr size_t kMinimumEventSize = 2 * kWordSize;  // Minimum event size (for PSD2)
constexpr size_t kAMaxEventSize = 4 * kWordSize;     // AMax event size (4 words)
constexpr size_t kMaxChannelNumber = 127;            // Maximum channel number
constexpr size_t kMaxWaveformSamples = 65536;        // Reasonable waveform size limit
}  // namespace Validation

// ============================================================================
// Multiplication Factor Encoding
// ============================================================================
namespace MultiplicationFactor
{
constexpr uint32_t kFactor1 = 0;   // Encoded as 0
constexpr uint32_t kFactor4 = 1;   // Encoded as 1
constexpr uint32_t kFactor8 = 2;   // Encoded as 2
constexpr uint32_t kFactor16 = 3;  // Encoded as 3
}  // namespace MultiplicationFactor

} // namespace AMaxConstants
} // namespace Digitizer
} // namespace DELILA