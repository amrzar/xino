/**
 * @file smccc.hpp
 * @brief Arm SMCCC (SMC Calling Convention) helpers.
 *
 * This header provides an implementation of the Arm
 * **SMC Calling Convention (SMCCC)** for AArch64.
 * See https://developer.arm.com/documentation/den0028/gb/?lang=en
 *
 * @author Amirreza Zarrabi
 * @date 2026
 */

#ifndef __SMCCC_HPP__
#define __SMCCC_HPP__

#include <cstddef>
#include <cstdint>

namespace xino::smccc {

/* 2.5 Function Identifiers (FIDs). */

using fid_t = std::uint32_t;

/* 2.3 Fast Calls and Yielding Calls. */

enum class call_type : fid_t { yielding = 0, fast = 1 };

/* 2.4 32-bit and 64-bit conventions. */

enum class call_conv : fid_t { smccc32 = 0, smccc64 = 1 };

/**
 * @brief SMCCC argument/result register block (AArch64).
 *
 * This structure models the SMCCC calling convention register set used for
 * SMC/HVC calls on AArch64. It provides storage for the argument registers
 * supplied to the call and the result registers returned from the call.
 *
 * Register mapping:
 *  - `x[0]`  -> X0 / W0 : FID on entry; return code/value on exit.
 *  - `x[1]`  -> X1 / W1 : Argument 1 / Result 1
 *  -  ...
 *  - `x[6]`  -> X6 / W6 : Argument 6 / Result 6
 *                         (Session ID - 2.13 Session ID)
 *  - `x[7]`  -> X7 / W7 : Argument 7 / Result 7
 *                         (Client ID in bits[15:0] - 2.11 Client ID)
 *                         (Secure OS ID in bits[31:16] - 2.12 Secure OS ID)
 *  - `x[8]`..`x[17]`    : Additional argument/result registers.
 *
 * Calling convention notes:
 *  - **SMCCC32 (SMC32/HVC32)**:
 *    - The FID is passed in **W0** and arguments are passed in **W1 - W7**.
 *    - Results are returned in **W0 - W7**.
 *    - Upper halves Xn[63:32] of Wn values are not part of the contract; in
 *      particular X0[63:32] is UNDEFINED on return for SMCCC32 calls.
 *
 *  - **SMCCC64 (SMC64/HVC64)**:
 *    - The FID is passed in **W0** and arguments are passed in **X1 - X17**.
 *    - Results are returned in **X0 - X17**.
 *
 * Alignment:
 *  - The object is @c alignas(16) so that paired loads/stores (LDP/STP) in the
 *    assembly conduit can operate efficiently while keeping stack/data
 *    alignment consistent with AArch64 ABI expectations.
 *
 * Usage:
 *  - Populate `x[0]` with the FID and `x[1]..` with arguments.
 *  - Call @ref xino::smccc::smccc_smc.
 *  - Read back results from `x[0]..` in the output block.
 */
struct alignas(16) args {
  // Table 3-1: Register Usage in AArch64 SMC32, HVC32, SMC64, and HVC64 calls.
  std::uint64_t x[18];
  // Accessors.
  constexpr std::uint64_t &operator[](std::size_t i) noexcept { return x[i]; }
  constexpr const std::uint64_t &operator[](std::size_t i) const noexcept {
    return x[i];
  }
};

/* See smccc.S. */
extern "C" void smccc_smc(const args *in, args *out) noexcept;

/* 2.5.1 Fast Calls. */

// Table 2-1: Bit usage in the SMC and HVC Function Identifier for Fast Call.
constexpr unsigned FID_TYPE_FAST_SHIFT{31}; // 1-bit;
constexpr unsigned FID_CC_64_SHIFT{30};     // 1-bit.
constexpr unsigned FID_OEN_SHIFT{24};       // 6-bits.
constexpr unsigned FID_SVE_HINT_SHIFT{16};  // 1-bit;

constexpr fid_t FID_TYPE_FAST_MASK{fid_t{1} << FID_TYPE_FAST_SHIFT};
constexpr fid_t FID_CC_64_MASK{fid_t{1} << FID_CC_64_SHIFT};
constexpr fid_t FID_OEN_MASK{fid_t{0b111111} << FID_OEN_SHIFT};
constexpr fid_t FID_MBZ_MASK{fid_t{0x00fe0000}}; // Bit 23:17 */
constexpr fid_t FID_SVE_HINT_MASK{fid_t{1} << FID_SVE_HINT_SHIFT};
constexpr fid_t FID_FUNC_MASK{fid_t{0x0000ffff}};

/* 2.5.2 Yielding Calls. */

// Per the SMCCC function-ID allocation, "yielding" only for Trusted OS calls.

/* 6 Function Identifier Ranges. */

// Table 6-1: SMC and HVC Services (Fast call services).
enum class oen : fid_t {
  arch = 0,
  cpu = 1,
  sip = 2,
  oem = 3,
  std_secure = 4,
  std_hypervisor = 5,
  vendor_hypervisor = 6,
  vendor_el3_monitor = 7,
  trusted_app_0 = 48, /* 48-49 */
  trusted_app_1 = 49,
  trusted_os_0 = 50, /* 50-63 */
  trusted_os_63 = 63
};

/**
 * 6.1 Allocation of values.
 *
 * - Only use yielding-call semantics for Trusted OS/TEE-style services.
 * - For PSCI/SDEI/FF-A/etc., those are allocated in the fast-call ranges,
 *   so issuing them as yielding calls is outside the allocated space
 *   and may get rejected/treated as unknown.
 *
 * Table 6-2: Allocated subranges of Function Identifier to different services.
 * --------------------------------------------------------------------------
 *  SMC Function Identifier | Service type
 * --------------------------------------------------------------------------
 *  0x02000000-0x1FFFFFFF   | Trusted OS Yielding Calls
 *  0x20000000-0x7FFFFFFF   | Trusted OS Yielding Calls (RESERVED)
 * --------------------------------------------------------------------------
 *  0x80000000-0x8000FFFF   | SMC32: Arm Architecture Calls
 *  0x81000000-0x8100FFFF   | SMC32: CPU Service Calls
 *  0x82000000-0x8200FFFF   | SMC32: SiP Service Calls
 *  0x83000000-0x8300FFFF   | SMC32: OEM Service Calls
 *  0x84000000-0x8400FFFF   | SMC32: Standard Service Calls
 *  0x85000000-0x8500FFFF   | SMC32: Standard Hypervisor Service Calls
 *  0x86000000-0x8600FFFF   | SMC32: Vendor Specific Hypervisor Service Calls
 *  0x87000000-0x8700FFFF   | SMC32: Vendor Specific EL3 Monitor Service Calls
 *  0x88000000-0xAF00FFFF   | RESERVED
 *  0xB0000000-0xB100FFFF   | SMC32: Trusted Application Calls
 *  0xB2000000-0xBF00FFFF   | SMC32: Trusted OS Calls
 *  0xC0000000-0xC000FFFF   | SMC64: Arm Architecture Calls
 *  0xC1000000-0xC100FFFF   | SMC64: CPU Service Calls
 *  0xC2000000-0xC200FFFF   | SMC64: SiP Service Calls
 *  0xC3000000-0xC300FFFF   | SMC64: OEM Service Calls
 *  0xC4000000-0xC400FFFF   | SMC64: Standard Service Calls
 *  0xC5000000-0xC500FFFF   | SMC64: Standard Hypervisor Service Calls
 *  0xC6000000-0xC600FFFF   | SMC64: Vendor Specific Hypervisor Service Calls
 *  0xC7000000-0xC700FFFF   | SMC64: Vendor Specific EL3 Monitor Service Calls
 *  0xC8000000-0xEF00FFFF   | RESERVED
 *  0xF0000000-0xF100FFFF   | SMC64: Trusted Application Calls
 *  0xF2000000-0xFF00FFFF   | SMC64: Trusted OS Calls
 */

[[nodiscard]] constexpr fid_t make_fast_fid(call_conv cc, oen owning_entity,
                                            std::uint32_t func) {
  return (fid_t{1} << FID_TYPE_FAST_SHIFT |
          (static_cast<fid_t>(cc) << FID_CC_64_SHIFT) |
          (static_cast<fid_t>(owning_entity) << FID_OEN_SHIFT) |
          (func & FID_FUNC_MASK));
}

[[nodiscard]] constexpr bool fid_is_fast(fid_t fid) noexcept {
  return (fid & FID_TYPE_FAST_MASK) != 0;
}

[[nodiscard]] constexpr bool fid_is_64(fid_t fid) noexcept {
  return (fid & FID_CC_64_MASK) != 0;
}

[[nodiscard]] constexpr fid_t fid_oen(fid_t fid) noexcept {
  return (fid & FID_OEN_MASK) >> FID_OEN_SHIFT;
}

[[nodiscard]] constexpr fid_t fid_func(fid_t fid) noexcept {
  return (fid & FID_FUNC_MASK);
}

[[nodiscard]] constexpr bool fid_sve_hint(fid_t fid) noexcept {
  return (fid & FID_SVE_HINT_MASK) != 0;
}

/** @brief Set/clear the SVE "no-live-state" hint bit (SMCCC v1.3+). */
[[nodiscard]] constexpr fid_t fid_with_sve_hint(fid_t fid, bool hint) noexcept {
  return hint ? (fid | FID_SVE_HINT_MASK) : (fid & ~FID_SVE_HINT_MASK);
}

/* 5.1 Error codes. */

using ret_t = std::int64_t;

/**
 * @brief Decode an SMCCC return value in X0 into a signed error/status code.
 *
 * SMCCC uses the same *numeric* error codes for both SMCCC32 and SMCCC64,
 * but the **width and sign-extension rules differ** depending on the calling
 * convention encoded in the Function Identifier:
 *
 * - **SMC64/HVC64 (SMCCC64)**: error codes are returned in **X0** as a
 *   **64-bit signed integer**.
 * - **SMC32/HVC32 (SMCCC32)**: error codes are returned in **W0** as a
 *   **32-bit signed integer**; **X0[63:32] is UNDEFINED**, so callers must
 *   **ignore the upper 32 bits** and sign-extend from 32-bit.
 *
 * @param fid The SMCCC Function Identifier used for the call (used to determine
 *            whether the call is SMCCC32 or SMCCC64).
 * @param x0 The raw value read from register X0 after the call.
 * @return The decoded signed return code (0 for success, negative for errors).
 */
[[nodiscard]] inline ret_t retcode_from_x0(fid_t fid,
                                           std::uint64_t x0) noexcept {
  if (fid_is_64(fid))
    return static_cast<ret_t>(x0);

  return static_cast<ret_t>(
      static_cast<std::int32_t>(static_cast<std::uint32_t>(x0)));
}

} // namespace xino::smccc

/* 7 Arm Architecture Calls. */

namespace xino::smccc::arch {

/** @brief Standard SMCCC return codes for Arm Architecture service calls. */
enum class ret : xino::smccc::ret_t {
  // Table 7-1: Return code and values.
  success = 0,        /**< Call completed successfully. */
  not_supported = -1, /**< The call is not supported by the implementation. */
  not_required = -2,  /**< The call is not required by the implementation. */
  invalid_parameter =
      -3 /**< One or more call parameters has a non-supported value. */
};

/* 7.2 SMCCC_VERSION. */

constexpr xino::smccc::fid_t SMCCC_VERSION{0x80000000};

struct smccc_version {
  std::uint32_t major;
  std::uint32_t minor;
};

/**
 * @brief Query the SMCCC version implemented.
 *
 * Issues `SMCCC_VERSION` and decodes the SMCCC return code.
 *
 * - On success, the implementation returns a signed 32-bit value where:
 *   - bits[30:16] = Major version
 *   - bits[15:0]  = Minor version
 *   - bit[31] must be zero.
 * - If the implementation returns `ret::not_supported`, the caller must treat
 *   this as indicating firmware that implements **SMCCC v1.0**.
 *
 * @return SMCCC major/minor version.
 */
[[nodiscard]] inline smccc_version version() noexcept {
  args in{}, out{};

  in[0] = SMCCC_VERSION;
  // Get the version info.
  xino::smccc::smccc_smc(&in, &out);

  // 7.2.3 Caller responsibilities.
  // NOT_SUPPORTED indicate presence of firmware that implements SMCCC v1.0.
  if (xino::smccc::retcode_from_x0(SMCCC_VERSION, out[0]) ==
      static_cast<xino::smccc::ret_t>(ret::not_supported)) {
    return smccc_version{1, 0};
  }

  // Bit[31] must be zero.
  std::uint32_t ver{static_cast<std::uint32_t>(out[0]) & 0x7FFFFFFFU};
  // Bits [30:16] Major version.
  // Bits [15:0] Minor version.
  return smccc_version{(ver >> 16) & 0x7FFFU, ver & 0xFFFFU};
}

/* 7.3 SMCCC_ARCH_FEATURES. */

constexpr xino::smccc::fid_t SMCCC_ARCH_FEATURES{0x80000001};

/**
 * @brief Query whether an Arm Architecture Service function is implemented.
 *
 * Issues `SMCCC_ARCH_FEATURES` with @p arch_func_id identifying an Arm
 * Architecture Service FID and returns the SMCCC status code.
 *
 * @param arch_func_id The Arm Architecture Service FID to query.
 *
 * @retval `ret::success` indicates the function is supported.
 * @retval A negative error code (for example `ret::not_supported`) indicates
 *         the function is not supported (or the parameter is invalid).
 * @retval A positive error code indicates the function is supported and
 * provides feature flags specific to the function.
 */
[[nodiscard]] inline xino::smccc::ret_t
arch_features(std::uint32_t arch_func_id) noexcept {
  args in{}, out{};

  in[0] = SMCCC_ARCH_FEATURES;
  in[1] = arch_func_id;
  // Get the features info.
  xino::smccc::smccc_smc(&in, &out);

  // - <0 Function not implemented or not in Arm Architecture Service range.
  // - Success Function implemented.
  // - >0 Function implemented. Function capabilities are indicated using
  //      feature flags specific to the function.
  return xino::smccc::retcode_from_x0(SMCCC_ARCH_FEATURES, out[0]);
}

/* 7.4 SMCCC_ARCH_SOC_ID. */

constexpr xino::smccc::fid_t SMCCC_ARCH_SOC_ID{0x80000002};

enum class soc_id_type : std::uint32_t { version = 0, revision = 1 };

/**
 * @brief Query the SoC identification value from firmware.
 *
 * Issues `SMCCC_ARCH_SOC_ID` with @p type selecting which SoC ID to return:
 *  - `soc_id_type::version`: return SoC version value.
 *  - `soc_id_type::revision`: return SoC revision value.
 *
 * @param type Which SoC ID value to query (version or revision).
 * @return On success a non-negative SoC-ID value.
 *         On failure a negative SMCCC error code.
 */
[[nodiscard]] inline xino::smccc::ret_t arch_soc_id(soc_id_type type) noexcept {
  args in{}, out{};

  in[0] = SMCCC_ARCH_SOC_ID;
  in[1] = static_cast<std::uint32_t>(type);
  // Get the soc_id info.
  xino::smccc::smccc_smc(&in, &out);

  return xino::smccc::retcode_from_x0(SMCCC_ARCH_SOC_ID, out[0]);
}

/* 7.8 SMCCC_ARCH_FEATURE_AVAILABILITY. */

constexpr xino::smccc::fid_t SMCCC_ARCH_FEATURE_AVAIL{0x80000003};

struct feat_avail_result {
  xino::smccc::ret_t status;
  std::uint64_t feat_bitmask;
};

/**
 * @brief Query feature availability bitmasks.
 *
 * Issues `SMCCC_ARCH_FEATURE_AVAILABILITY` with @p bitmask_selector selecting
 * which feature-availability bitmask to return.
 *
 * @param bitmask_selector Selector value defining which feature-availability
 *                         bitmask is requested (as defined by the SMCCC spec).
 * @return A @ref feat_avail_result containing:
 *         - status: SMCCC return code (0 on success, negative on error)
 *         - feat_bitmask: bitmask in X1 when status == SUCCESS, otherwise 0.
 */
[[nodiscard]] inline feat_avail_result
arch_feature_availability(std::uint64_t bitmask_selector) noexcept {
  args in{}, out{};

  in[0] = SMCCC_ARCH_FEATURE_AVAIL;
  in[1] = bitmask_selector;
  // Get the feature availability info.
  xino::smccc::smccc_smc(&in, &out);

  feat_avail_result r{};
  r.status = retcode_from_x0(SMCCC_ARCH_FEATURE_AVAIL, out[0]);
  r.feat_bitmask =
      (r.status == static_cast<xino::smccc::ret_t>(ret::success)) ? out[1] : 0;
  return r;
}

} // namespace xino::smccc::arch

#endif // __SMCCC_HPP__
