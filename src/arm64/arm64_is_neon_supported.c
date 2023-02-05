#include "funcs_arm64.h"

int
arm64_is_neon_supported()
{
    /**
     * https://developer.arm.com/documentation/den0024/a/AArch64-Floating-point-and-NEON
     *
     * ... Both floating-point and NEON are required in all standard ARMv8 implementations. However, implementations targeting specialized markets may support the following combinations:
     *  - No NEON or floating-point.
     *  - Full floating-point and SIMD support with exception trapping.
     *  - Full floating-point and SIMD support without exception trapping.
     */

    return 1;
}