#ifndef CRYPTO_HASH_H
#define CRYPTO_HASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PA_START_ROUND 0xf0

#if ASCON_HASH_BYTES == 32 && ASCON_HASH_ROUNDS == 12
#define IV(i) ASCON_HASH_IV##i
#define PB_START_ROUND 0xf0
#elif ASCON_HASH_BYTES == 32 && ASCON_HASH_ROUNDS == 8
#define IV(i) ASCON_HASHA_IV##i
#define PB_START_ROUND 0xb4
#elif ASCON_HASH_BYTES == 0 && ASCON_HASH_ROUNDS == 12
#define IV(i) ASCON_XOF_IV##i
#define PB_START_ROUND 0xf0
#elif ASCON_HASH_BYTES == 0 && ASCON_HASH_ROUNDS == 8
#define IV(i) ASCON_XOFA_IV##i
#define PB_START_ROUND 0xb4
#endif

int crypto_hash(unsigned char* out, const unsigned char* in, unsigned long long inlen);

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_HASH_H