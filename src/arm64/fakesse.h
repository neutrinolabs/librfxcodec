#include <arm_neon.h>

// movd
// vld1q_dup_s32(src)
#define load_dword(dst, src) { dst = vdupq_n_u8(0); ((int32_t *) &dst)[0] = src; }
#define load_dword_dup(dst, src) dst = vld1q_dup_s16(&src)
// #define load_dword_dup(dst, src) dst = vld1q_dup_s32(&src)

// movd
#define store_dword(dst, src) dst = ((int32_t *) &src)[0]

// mov (64-bit)
// FIXME: movq 타입 다를수도 있음 문서 보고 고쳐라
#define movq(dst, src) { dst = vdupq_n_u8(0); *((int64_t *) &dst) = src; }
// vextq_s8(vextq_s8(vdupq_n_s8(0), vld1q_s8(src), 8), vdupq_n_s8(0), 8)
// vld1q_s16(src)

// FIXME: vld1q하고 vld1q_dup를 혼용해서 사용해야 함
// #define movdqa(dst, src) dst = vld1q_dup_s16(src) // vst1q_s16을 써야 하는게 아닌지?
#define movdqa(dst, src) vst1q_u8((uint8_t *) dst, src)

/**
 * loads double quadword.
 * equals to movdqa(xmmN, ptr)
 */
#define load_dqword(dst, src) dst = vld1q_u8(src)

#define punpcklbw(dst, src) dst = vzipq_u8(dst, src).val[0]
// vtrnq_u8(dst, src).val[0]
// vzipq_s8(src, dst).val[0]
#define packusdw(vec1, vec2) vec1 = vmovn_high_u32(vmovn_u32(vec1), vec2)

#define pand(vec1, vec2) vec1 = vandq_u8(vec1, vec2)
#define por(vec1, vec2) vec1 = vorrq_u8(vec1, vec2)

#define paddw(vec1, vec2) vec1 = vaddq_s16(vec1, vec2)
#define psubw(vec1, vec2) vec1 = vsubq_s16(vec1, vec2)


// FIXME: psllw != pslaw (fixed)
#define psllw(vec, amount) vec = vshlq_n_u16(vec, amount)
#define psraw(vec, amount) vec = vshrq_n_s16(vec, amount)

// FIXME: vmul_n_s16에서 vec2가 바뀌는지 확인해야 함 (fixed)
// #define psraw_vec(vec1, vec2) vec1 = vshlq_s16(vec1, vmulq_n_s16(vec2, -1))
#define psraw_vec(vec1, vec2) vec1 = vshlq_s16(vec1, vmulq_n_s16(vec2, -1));

// FIXME: 부정확한 비트 시프팅
//        128bit 레지스터 째로 시프팅 하는 방법이.. (fixed)
#define pslldq(vec, amount) vec = vextq_u8(vdupq_n_u8(0), vec, 16 - amount)
#define psrldq(vec, amount) vec = vextq_u8(vec, vdupq_n_u8(0), amount)

