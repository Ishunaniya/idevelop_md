#include "evs/winch_ice.h"
#include <chrono>

namespace evs {
namespace winch_ice {
uint32_t monotonic_timestamp_ms() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}
} // namespace winch_ice
} // namespace evs
