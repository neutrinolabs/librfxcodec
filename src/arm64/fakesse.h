/**
 * fakesse.h - simulates amd64 SSE instructions on arm64 NEON
 *
 * @author Gyuhwan Park <unstabler@unstabler.pl>
 */

#include <arm_neon.h>

/**
 * loads & duplicates single word(int16_t) into xmmN vector
 *
 * @example 
 *      int16_t value = 123;
 *      
 *      // xmm1 -> int16x8_t {123, 123, 123, 123, 123, 123, 123, 123}
 *      load_word_dup(cpu_context.xmm1, value); 
 */
#define load_word_dup(dst, src) dst = vld1q_dup_s16(&src)

//
/**
 * loads single dword(int32_t) into xmmN vector
 * equals to movd xmmN, dwordval
 */
#define load_dword(dst, src) { dst = vdupq_n_u8(0); ((int32_t *) &dst)[0] = src; }

/**
 * stores single dword(int32_t) into specified memory address
 * equals to movd [ptr], xmmN
 */
#define store_dword(dst, src) dst = ((int32_t *) &src)[0]

/**
 * loads single quadword(int64_t) into xmmN vector
 * equals to movq xmmN, qwordval
 */ 
#define movq(dst, src) { dst = vdupq_n_u8(0); *((int64_t *) &dst) = src; }

/**
 * stores double quadword-sized vector/memory to destination
 *
 * equals to 
 *  - movdqa xmmN, [ptr]
 *  - movdqa [ptr], xmmN
 *  - movdqa xmmX, xmmY
 * @note there may be performance-related issues since te implementation doesn't care about memory alignment
 */
#define movdqa(dst, src) vst1q_u8((uint8_t *) dst, src)

/**
 * loads double quadword-sized (int64_t * 2) vector/memory to destination
 *
 * equals to 
 *  - movdqa xmmN, [ptr]
 *  - movdqa [ptr], xmmN
 *  - movdqa xmmX, xmmY
 * @note there may be performance-related issues since te implementation doesn't care about memory alignment
 */
#define load_dqword(dst, src) dst = vld1q_u8(src)

/**
 * @example
 *     xmm1 = uint8x16_t {  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12,  13,  14,  15,  16 };
 *     xmm2 = uint8x16_t { -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16 };
 *
 *     // xmm1 -> uint8x16_t { 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7, 8, -8 }
 *     punpcklbw(xmm1, xmm2);
 */
#define punpcklbw(dst, src) dst = vzipq_u8(dst, src).val[0]

/**
 * 이거 맞나..?
 * @example
 *     xmm1 = uint32x4_t {  123, 456, 789, 1234 };
 *     xmm2 = uint32x4_t {   42,  42,  42,   42 };
 *
 *     // xmm1 -> uint16x8_t { 123, 123, 123, 123, 42, 42, 42, 42 }
 *     packusdw(xmm1, xmm2);
 */
#define packusdw(vec1, vec2) vec1 = vmovn_high_u32(vmovn_u32(vec1), vec2)

/**
 * performs logical AND operation
 */
#define pand(vec1, vec2) vec1 = vandq_u8(vec1, vec2)

/**
 * performs logical OR operation
 */
#define por(vec1, vec2) vec1 = vorrq_u8(vec1, vec2)

#define paddw(vec1, vec2) vec1 = vaddq_s16(vec1, vec2)
#define psubw(vec1, vec2) vec1 = vsubq_s16(vec1, vec2)


/**
 * performs bit shift operation on each word-sized elements (logical; left)
 */
#define psllw(vec, amount) vec = vshlq_n_u16(vec, amount)

/**
 * performs bit shift operation on each word-sized elements (arithmetic; right)
 */
#define psraw(vec, amount) vec = vshrq_n_s16(vec, amount)

/**
 * performs bit shift operation on each word-sized elements (arithmetic; right)
 *
 * equals to 
 *   - psraw xmmX, xmmY
 */
#define psraw_vec(vec1, vec2) vec1 = vshlq_s16(vec1, vmulq_n_s16(vec2, -1));

/**
 * performs byte shift on entire double quadword-sized (int64_t * 2) vector
 *
 * @example
 *     xmm1 = uint8x16_t { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
 *
 *     // xmm1 -> uint8x16_t { 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
 *     pslldq(xmm1, 4); 
 */
#define pslldq(vec, amount) vec = vextq_u8(vdupq_n_u8(0), vec, 16 - amount)
#define psrldq(vec, amount) vec = vextq_u8(vec, vdupq_n_u8(0), amount)

