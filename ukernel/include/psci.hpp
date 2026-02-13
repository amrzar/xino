
#ifndef __PSCI_HPP__
#define __PSCI_HPP__

#include <cstdint>
#include <mm.hpp>
#include <smccc.hpp>

namespace xino::fw::psci {
using namespace xino::smccc;

constexpr fid_t psci_fn32(unsigned n) {
  return make_fast_fid(call_conv::smccc32, oen::std_secure, n);
}

constexpr fid_t psci_fn64(unsigned n) {
  return make_fast_fid(call_conv::smccc32, oen::std_secure, n);
}

/* 5.1.1 PSCI_VERSION (0x8400'0000). */
constexpr fid_t PSCI_0_2_FN_PSCI_VERSION{psci_fn32(0x0)};
/* 5.1.2 CPU_SUSPEND (0x8400'0001, 0xC400'0001). */
constexpr fid_t PSCI_0_2_FN_CPU_SUSPEND{psci_fn32(0x1)};
constexpr fid_t PSCI_0_2_FN64_CPU_SUSPEND{psci_fn64(0x1)};
/* 5.1.3 CPU_OFF (0x8400'0002). */
constexpr fid_t PSCI_0_2_FN_CPU_OFF{psci_fn32(0x2)};
/* 5.1.4 CPU_ON (0x8400'0003, 0xC400'0003). */
constexpr fid_t PSCI_0_2_FN_CPU_ON{psci_fn32(0x3)};
constexpr fid_t PSCI_0_2_FN64_CPU_ON{psci_fn64(0x3)};
/* 5.1.5 AFFINITY_INFO (0x8400'0004, 0xC400'0004). */
constexpr fid_t PSCI_0_2_FN_AFFINITY_INFO{psci_fn32(0x4)};
constexpr fid_t PSCI_0_2_FN64_AFFINITY_INFO{psci_fn64(0x4)};
/* 5.1.6 MIGRATE (0x8400'0005, 0xC400'0005). */
constexpr fid_t PSCI_0_2_FN_MIGRATE{psci_fn32(0x5)};
constexpr fid_t PSCI_0_2_FN64_MIGRATE{psci_fn64(0x5)};
/* 5.1.7 MIGRATE_INFO_TYPE (0x8400'0006). */
constexpr fid_t PSCI_0_2_FN_MIGRATE_INFO_TYPE{psci_fn32(0x6)};
/* 5.1.8 MIGRATE_INFO_UP_CPU (0x8400'0007, 0xC400'0007). */
constexpr fid_t PSCI_0_2_FN_MIGRATE_INFO_UP_CPU{psci_fn32(0x7)};
constexpr fid_t PSCI_0_2_FN64_MIGRATE_INFO_UP_CPU{psci_fn64(0x7)};
/* 5.1.9 SYSTEM_OFF (0x8400'0008). */
constexpr fid_t PSCI_0_2_FN_SYSTEM_OFF{psci_fn32(0x8)};
/* 5.1.10 SYSTEM_RESET (0x8400'0009). */
constexpr fid_t PSCI_0_2_FN_SYSTEM_RESET{psci_fn32(0x9)};
/* 5.1.11 SYSTEM_RESET2 (0x8400'0012, 0xC400'0012). */
constexpr fid_t PSCI_1_1_FN_SYSTEM_RESET2{psci_fn32(0x12)};
constexpr fid_t PSCI_1_1_FN64_SYSTEM_RESET2{psci_fn64(0x12)};
/* 5.1.12 MEM_PROTECT (0x8400'0013). */
constexpr fid_t PSCI_1_1_FN_MEM_PROTECT{psci_fn32(0x13)};
/* 5.1.13 MEM_PROTECT_CHECK_RANGE (0x8400'0014, 0xC400'0014). */
constexpr fid_t PSCI_1_1_FN_MEM_PROTECT_CHECK_RANGE{psci_fn32(0x14)};
constexpr fid_t PSCI_1_1_FN64_MEM_PROTECT_CHECK_RANGE{psci_fn64(0x14)};
/* 5.1.14 PSCI_FEATURES (0x8400'000A). */
constexpr fid_t PSCI_1_0_FN_PSCI_FEATURES{psci_fn32(0xa)};
/* 5.1.15 CPU_FREEZE (0x8400'000B). */
constexpr fid_t PSCI_1_0_FN_CPU_FREEZE{psci_fn32(0xb)};
/* 5.1.16 CPU_DEFAULT_SUSPEND (0x8400'000C, 0xC400'000C). */
constexpr fid_t PSCI_1_0_FN_CPU_DEFAULT_SUSPEND{psci_fn32(0xc)};
constexpr fid_t PSCI_1_0_FN64_CPU_DEFAULT_SUSPEND{psci_fn64(0xc)};
/* 5.1.17 NODE_HW_STATE (0x8400'000D, 0xC400'000D). */
constexpr fid_t PSCI_1_0_FN_NODE_HW_STATE{psci_fn32(0xd)};
constexpr fid_t PSCI_1_0_FN64_NODE_HW_STATE{psci_fn64(0xd)};
/* 5.1.18 SYSTEM_SUSPEND (0x8400'000E, 0xC400'000E). */
constexpr fid_t PSCI_1_0_FN_SYSTEM_SUSPEND{psci_fn32(0xe)};
constexpr fid_t PSCI_1_0_FN64_SYSTEM_SUSPEND{psci_fn64(0xe)};
/* 5.1.19 PSCI_SET_ SUSPEND_MODE (0x8400'000F). */
constexpr fid_t PSCI_1_0_FN_SET_SUSPEND_MODE{psci_fn32(0xf)};
/* 5.1.20 PSCI_STAT_RESIDENCY (0x8400'0010, 0xC400'0010). */
constexpr fid_t PSCI_1_0_FN_STAT_RESIDENCY{psci_fn32(0x10)};
constexpr fid_t PSCI_1_0_FN64_STAT_RESIDENCY{psci_fn64(0x10)};
/* 5.1.21 PSCI_STAT_COUNT (0x8400'0011, 0xC400'0011). */
constexpr fid_t PSCI_1_0_FN_STAT_COUNT{psci_fn32(0x11)};
constexpr fid_t PSCI_1_0_FN64_STAT_COUNT{psci_fn64(0x11)};

/* 5.2.2 Return error codes. */

enum : smccc_ret_t {
  SUCCESS = 0,
  NOT_SUPPORTED = -1,
  INVALID_PARAMETER = -2,
  DENIED = -3,
  ALREADY_ON = -4,
  ON_PENDING = -5,
  INTERNAL_FAILURE = -6,
  NOT_PRESENT = -7,
  DISABLED = -8,
  INVALID_ADDRESS = -9
};

/* 5.3 PSCI_VERSION. */

struct psci_version {
  std::uint32_t major;
  std::uint32_t minor;
};

[[nodiscard]] inline psci_version version() noexcept {
  args in{}, out{};

  in[0] = PSCI_0_2_FN_PSCI_VERSION;
  // Get the version info.
  smccc_smc(&in, &out);

  const std::uint32_t v{static_cast<std::uint32_t>(out[0])};
  // Bits [31:16] Major version.
  // Bits [15:0] Minor version.
  return psci_version{(v >> 16) & 0xffffU, v & 0xffffU};
}

/* 5.15 PSCI_FEATURES. */

// Table 11 Return values if a function is implemented.
constexpr std::uint32_t PSCI_FEATURES_CPU_SUSPEND_FORMAT_SHIFT{1}; // 1-bit.
constexpr std::uint32_t PSCI_FEATURES_CPU_SUSPEND_MODE_SHIFT{0};   // 1-bit.

[[nodiscard]] inline smccc_ret_t features(std::uint32_t psci_func_id) noexcept {
  args in{}, out{};

  in[0] = PSCI_1_0_FN_PSCI_FEATURES;
  in[1] = psci_func_id;
  // Get the feature info.
  smccc_smc(&in, &out);

  // 5.15.1 Intended use:
  // NOT_SUPPORTED, if the function is not implemented.
  // A set of feature flag bits.
  return retcode_from_x0(PSCI_1_0_FN_PSCI_FEATURES, out[0]);
}

/* 5.6 CPU_ON. */

[[nodiscard]] inline smccc_ret_t cpu_on(std::uint64_t target_mpidr,
                                        xino::mm::phys_addr entry_pa,
                                        std::uint64_t context_id) noexcept {
  args in{}, out{};

  in[0] = PSCI_0_2_FN64_CPU_ON;
  in[1] = target_mpidr;
  in[2] = static_cast<xino::mm::phys_addr::value_type>(entry_pa);
  in[3] = context_id;
  // Boot a CPU at entry_pa.
  smccc_smc(&in, &out);

  return retcode_from_x0(PSCI_0_2_FN64_CPU_ON, out[0]);
}

/* 5.5 CPU_OFF. */

[[nodiscard]] inline smccc_ret_t cpu_off() noexcept {
  args in{}, out{};

  in[0] = PSCI_0_2_FN_CPU_OFF;
  // Shutdown a CPU.
  smccc_smc(&in, &out);

  return retcode_from_x0(PSCI_0_2_FN_CPU_OFF, out[0]);
}

/* 5.4 CPU_SUSPEND. */

[[nodiscard]] inline smccc_ret_t
cpu_suspend(std::uint32_t power_state, xino::mm::phys_addr entry_pa,
            std::uint64_t context_id) noexcept {}

} // namespace xino::fw::psci

#endif // __PSCI_HPP__
