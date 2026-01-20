Do not repeat the original. Do not use markdown syntax in the response. Do not output the original file. Do not explain your changes. Do not output any other text.

----- BEGIN SOLUTION -----
/* sha256.c
 *
 * Copyright (C) 2006-2023 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* For more info on the algorithm, see https://tools.ietf.org/html/rfc6234
 *
 * For more information on NIST FIPS PUB 180-4, see
 * https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
 */

/*

DESCRIPTION
This library provides the interface to SHA-256 secure hash algorithms.
SHA-256 performs processing on message blocks to produce a final hash digest
output. It can be used to hash a message, M, having a length of L bits,
where 0 <= L < 2^64.

Note that in some cases, hardware acceleration may be enabled, depending
on the specific device platform.

*/

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/types.h>

/*
 * SHA256 Build Options:
 * USE_SLOW_SHA256:            Reduces code size by not partially unrolling
                                (~2KB smaller and ~25% slower) (default OFF)
 * WOLFSSL_SHA256_BY_SPEC:     Uses the Ch/Maj based on SHA256 specification
                                (default ON)
 * WOLFSSL_SHA256_ALT_CH_MAJ:  Alternate Ch/Maj that is easier for compilers to
                                optimize and recognize as SHA256 (default OFF)
 * SHA256_MANY_REGISTERS:      A SHA256 version that keeps all data in registers
                                and partial unrolled (default OFF)
 */

/* Default SHA256 to use Ch/Maj based on specification */
#if !defined(WOLFSSL_SHA256_BY_SPEC) && !defined(WOLFSSL_SHA256_ALT_CH_MAJ)
    #define WOLFSSL_SHA256_BY_SPEC
#endif


#if !defined(NO_SHA256) && (!defined(WOLFSSL_ARMASM) && \
    !defined(WOLFSSL_ARMASM_NO_NEON))

#if defined(HAVE_FIPS) && defined(HAVE_FIPS_VERSION) && (HAVE_FIPS_VERSION >= 2)
    /* set NO_WRAPPERS before headers, use direct internal f()s not wrappers */
    #define FIPS_NO_WRAPPERS

    #ifdef USE_WINDOWS_API
        #pragma code_seg(".fipsA$d")
        #pragma const_seg(".fipsB$d")
    #endif
#endif

#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/cpuid.h>
#include <wolfssl/wolfcrypt/hash.h>

#ifdef WOLF_CRYPTO_CB
    #include <wolfssl/wolfcrypt/cryptocb.h>
#endif

#ifdef WOLFSSL_IMXRT1170_CAAM
    #include <wolfssl/wolfcrypt/port/caam/wolfcaam_fsl_nxp.h>
#endif


/* determine if we are using Espressif SHA hardware acceleration */
#undef WOLFSSL_USE_ESP32_CRYPT_HASH_HW
#if defined(WOLFSSL_ESP32_CRYPT) && \
    !defined(NO_WOLFSSL_ESP32_CRYPT_HASH)
    /* define a single keyword for simplicity & readability
     *
     * by default the HW acceleration is on for ESP32-WROOM32
     * but individual components can be turned off.
     */
    #define WOLFSSL_USE_ESP32_CRYPT_HASH_HW
#else
    #undef WOLFSSL_USE_ESP32_CRYPT_HASH_HW
#endif

#ifdef WOLFSSL_ESPIDF
    /* Define the ESP_LOGx(TAG,  WOLFSSL_ESPIDF_BLANKLINE_MESSAGE value for output messages here.
    **
    ** Beware of possible conflict in test.c (that one now named TEST_TAG)
    */
    #if defined(WOLFSSL_USE_ESP32_CRYPT_HASH_HW) && \
       !defined(NO_WOLFSSL_ESP32_CRYPT_HASH_SHA256)
        static const char* TAG = "wc_sha256";
    #endif
#endif

#if defined(WOLFSSL_TI_HASH)
    /* #include <wolfcrypt/src/port/ti/ti-hash.c> included by wc_port.c */
#elif defined(WOLFSSL_CRYPTOCELL)
    /* wc_port.c includes wolfcrypt/src/port/arm/cryptoCellHash.c */

#elif defined(WOLFSSL_PSOC6_CRYPTO)


#else

#include <wolfssl/wolfcrypt/logging.h>

#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif

#ifdef WOLFSSL_DEVCRYPTO_HASH
    #include <wolfssl/wolfcrypt/port/devcrypto/wc_devcrypto.h>
#endif
#if defined(WOLFSSL_SE050) && defined(WOLFSSL_SE050_HASH)
    #include <wolfssl/wolfcrypt/port/nxp/se050_port.h>
#endif


#if defined(WOLFSSL_X86_64_BUILD) && defined(USE_INTEL_SPEEDUP)
    #if defined(__GNUC__) && ((__GNUC__ < 4) || \
                              (__GNUC__ == 4 && __GNUC_MINOR__ <= 8))
        #undef  NO_AVX2_SUPPORT
        #define NO_AVX2_SUPPORT
    #endif
    #if defined(__clang__) && ((__clang_major__ < 3) || \
                               (__clang_major__ == 3 && __clang_minor__ <= 5))
        #define NO_AVX2_SUPPORT
    #elif defined(__clang__) && defined(NO_AVX2_SUPPORT)
        #undef NO_AVX2_SUPPORT
    #endif

    #define HAVE_INTEL_AVX1
    #ifndef NO_AVX2_SUPPORT
        #define HAVE_INTEL_AVX2
    #endif
#else
    #undef HAVE_INTEL_AVX1
    #undef HAVE_INTEL_AVX2
#endif /* WOLFSSL_X86_64_BUILD && USE_INTEL_SPEEDUP */

#if defined(HAVE_INTEL_AVX2)
    #define HAVE_INTEL_RORX
#endif


#if defined(LITTLE_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA)
    #if ( defined(CONFIG_IDF_TARGET_ESP32C2) || \
          defined(CONFIG_IDF_TARGET_ESP8684) || \
          defined(CONFIG_IDF_TARGET_ESP32C3) || \
          defined(CONFIG_IDF_TARGET_ESP32C6)    \
        ) && \
        defined(WOLFSSL_ESP32_CRYPT) &&         \
        !defined(NO_WOLFSSL_ESP32_CRYPT_HASH) && \
        !defined(NO_WOLFSSL_ESP32_CRYPT_HASH_SHA256)
        /* For Espressif RISC-V Targets, we *may* need to reverse bytes
         * depending on if HW is active or not. */
        #define SHA256_REV_BYTES(ctx) \
            (esp_sha_need_byte_reversal(ctx))
    #endif
#endif
#ifndef SHA256_REV_BYTES
    #if defined(LITTLE_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA)
        #define SHA256_REV_BYTES(ctx)       1
    #else
        #define SHA256_REV_BYTES(ctx)       0
    #endif
#endif
#if defined(LITTLE_ENDIAN_ORDER) && !defined(FREESCALE_MMCAU_SHA) && \
        defined(WOLFSSL_X86_64_BUILD) && defined(USE_INTEL_SPEEDUP) && \
        (defined(HAVE_INTEL_AVX1) || defined(HAVE_INTEL_AVX2))
    #define SHA256_UPDATE_REV_BYTES(ctx) \
        (!IS_INTEL_AVX1(intel_flags) && !IS_INTEL_AVX2(intel_flags))
#else
    #define SHA256_UPDATE_REV_BYTES(ctx)    SHA256_REV_BYTES(ctx)
#endif


#if !defined(WOLFSSL_PIC32MZ_HASH) && !defined(STM32_HASH_SHA2) && \
    (!defined(WOLFSSL_IMX6_CAAM) || defined(NO_IMX6_CAAM_HASH) || \
     defined(WOLFSSL_QNX_CAAM)) && \
    !defined(WOLFSSL_AFALG_HASH) && !defined(WOLFSSL_DEVCRYPTO_HASH) && \
    (!defined(WOLFSSL_ESP32_CRYPT) || defined(NO_WOLFSSL_ESP32_CRYPT_HASH)) && \
    ((!defined(WOLFSSL_RENESAS_TSIP_TLS) && \
      !defined(WOLFSSL_RENESAS_TSIP_CRYPTONLY)) || \
     defined(NO_WOLFSSL_RENESAS_TSIP_CRYPT_HASH)) && \
    !defined(WOLFSSL_PSOC6_CRYPTO) && !defined(WOLFSSL_IMXRT_DCP) && !defined(WOLFSSL_SILABS_SE_ACCEL) && \
    !defined(WOLFSSL_KCAPI_HASH) && !defined(WOLFSSL_SE050_HASH) && \
    ((!defined(WOLFSSL_RENESAS_SCEPROTECT) && \
      !defined(WOLFSSL_RENESAS_RSIP)) \
      || defined(NO_WOLFSSL_RENESAS_FSPSM_HASH)) && \
    (!defined(WOLFSSL_HAVE_PSA) || defined(WOLFSSL_PSA_NO_HASH)) && \
    !defined(WOLFSSL_RENESAS_RX64_HASH)


static int InitSha256(wc_Sha256* sha256)
{
    XMEMSET(sha256->digest, 0, sizeof(sha256->digest));
    sha256->digest[0] = 0x6A09E667L;
    sha256->digest[1] = 0xBB67AE85L;
    sha256->digest[2] = 0x3C6EF372L;
    sha256->digest[3] = 0xA54FF53AL;
    sha256->digest[4] = 0x510E527FL;
    sha256->digest[5] = 0x9B05688CL;
    sha256->digest[6] = 0x1F83D9ABL;
    sha256->digest[7] = 0x5BE0CD19L;

    sha256->buffLen = 0;
    sha256->loLen   = 0;
    sha256->hiLen   = 0;
#ifdef WOLFSSL_HASH_FLAGS
    sha256->flags = 0;
#endif
#ifdef WOLFSSL_HASH_KEEP
    sha256->msg  = NULL;
    sha256->len  = 0;
    sha256->used = 0;
#endif

#ifdef WOLF_CRYPTO_CB
    sha256->devId = wc_CryptoCb_DefaultDevID();
#endif

#ifdef WOLFSSL_MAXQ10XX_CRYPTO
    XMEMSET(&sha256->maxq_ctx, 0, sizeof(sha256->maxq_ctx));
#endif

#ifdef HAVE_ARIA
    sha256->hSession = NULL;
#endif

    return 0;
}
#endif


/* Hardware Acceleration */
#if defined(WOLFSSL_X86_64_BUILD) && defined(USE_INTEL_SPEEDUP) && \
                          (defined(HAVE_INTEL_AVX1) || defined(HAVE_INTEL_AVX2))

    /* in case intel instructions aren't available, plus we need the K[] global */
    #define NEED_SOFT_SHA256

    /*****
    Intel AVX1/AVX2 Macro Control Structure

    #define HAVE_INTEL_AVX1
    #define HAVE_INTEL_AVX2

    #define HAVE_INTEL_RORX


    int InitSha256(wc_Sha256* sha256) {
         Save/Recover XMM, YMM
         ...
    }

    #if defined(HAVE_INTEL_AVX1)|| defined(HAVE_INTEL_AVX2)
      Transform_Sha256(); Function prototype
    #else
      Transform_Sha256() {   }
      int Sha256Final() {
         Save/Recover XMM, YMM
         ...
      }
    #endif

    #if defined(HAVE_INTEL_AVX1)|| defined(HAVE_INTEL_AVX2)
        #if defined(HAVE_INTEL_RORX
             #define RND with rorx instruction
        #else
            #define RND
        #endif
    #endif

    #if defined(HAVE_INTEL_AVX1)

       #define XMM Instructions/inline asm

       int Transform_Sha256() {
           Stitched Message Sched/Round
        }

    #elif defined(HAVE_INTEL_AVX2)

      #define YMM Instructions/inline asm

      int Transform_Sha256() {
          More granular Stitched Message Sched/Round
      }

    #endif

    */

    /* Each platform needs to query info type 1 from cpuid to see if aesni is
     * supported. Also, let's setup a macro for proper linkage w/o ABI conflicts
     */

    /* #if defined(HAVE_INTEL_AVX1/2) at the tail of sha256 */
    static int Transform_Sha256(wc_Sha256* sha256, const byte* data);

#ifdef __cplusplus
    extern "C" {
#endif

        extern int Transform_Sha256_SSE2_Sha(wc_Sha256 *sha256,
                                             const byte* data);
        extern int Transform_Sha256_SSE2_Sha_Len(wc_Sha256* sha256,
                                                 const byte* data, word32 len);
    #if defined(HAVE_INTEL_AVX1)
        extern int Transform_Sha256_AVX1_Sha(wc_Sha256 *sha256,
                                             const byte* data);
        extern int Transform_Sha256_AVX1_Sha_Len(wc_Sha256* sha256,
                                                 const byte* data, word32 len);
        extern int Transform_Sha256_AVX1(wc_Sha256 *sha256, const byte* data);
        extern int Transform_Sha256_AVX1_Len(wc_Sha256* sha256,
                                             const byte* data, word32 len);
    #endif
    #if defined(HAVE_INTEL_AVX2)
        extern int Transform_Sha256_AVX2(wc_Sha256 *sha256, const byte* data);
        extern int Transform_Sha256_AVX2_Len(wc_Sha256* sha256,
                                             const byte* data, word32 len);
        #ifdef HAVE_INTEL_RORX
        extern int Transform_Sha256_AVX1_RORX(wc_Sha256 *sha256, const byte* data);
        extern int Transform_Sha256_AVX1_RORX_Len(wc_Sha256* sha256,
                                                  const byte* data, word32 len);
        extern int Transform_Sha256_AVX2_RORX(wc_Sha256 *sha256, const byte* data);
        extern int Transform_Sha256_AVX2_RORX_Len(wc_Sha256* sha256,
                                                  const byte* data, word32 len);
        #endif /* HAVE_INTEL_RORX */
    #endif /* HAVE_INTEL_AVX2 */

#ifdef __cplusplus
    }  /* extern "C" */
#endif

    static int (*Transform_Sha256_p)(wc_Sha256* sha256, const byte* data);
                                                       /* = _Transform_Sha256 */
    static int (*Transform_Sha256_Len_p)(wc_Sha256* sha256, const byte* data,
                                         word32 len);
                                                                    /* = NULL */
    static int transform_check = 0;
    static word32 intel_flags;
    static int Transform_Sha256_is_vectorized = 0;

    static WC_INLINE int inline_XTRANSFORM(wc_Sha256* S, const byte* D) {
        int ret;
        ret = (*Transform_Sha256_p)(S, D);
        return ret;
    }
#define XTRANSFORM(...) inline_XTRANSFORM(__VA_ARGS__)

    static WC_INLINE int inline_XTRANSFORM_LEN(wc_Sha256* S, const byte* D, word32 L) {
        int ret;
        ret = (*Transform_Sha256_Len_p)(S, D, L);
        return ret;
    }
#define XTRANSFORM_LEN(...) inline_XTRANSFORM_LEN(__VA_ARGS__)

    static void Sha256_SetTransform(void)
    {

        if (transform_check)
            return;

        intel_flags = cpuid_get_flags();

        if (IS_INTEL_SHA(intel_flags)) {
        #ifdef HAVE_INTEL_AVX1
            if (IS_INTEL_AVX1(intel_flags)) {
                Transform_Sha256_p = Transform_Sha256_AVX1_Sha;
                Transform_Sha256_Len_p = Transform_Sha256_AVX1_Sha_Len;
                Transform_Sha256_is_vectorized = 1;
            }
            else
        #endif
            {
                Transform_Sha256_p = Transform_Sha256_SSE2_Sha;
                Transform_Sha256_Len_p = Transform_Sha256_SSE2_Sha_Len;
                Transform_Sha256_is_vectorized = 1;
            }
        }
        else
    #ifdef HAVE_INTEL_AVX2
        if (IS_INTEL_AVX2(intel_flags)) {
        #ifdef HAVE_INTEL_RORX
            if (IS_INTEL_BMI2(intel_flags)) {
                Transform_Sha256_p = Transform_Sha256_AVX2_RORX;
                Transform_Sha256_Len_p = Transform_Sha256_AVX2_RORX_Len;
                Transform_Sha256_is_vectorized = 1;
            }
            else
        #endif
            {
                Transform_Sha256_p = Transform_Sha256_AVX2;
                Transform_Sha256_Len_p = Transform_Sha256_AVX2_Len;
                Transform_Sha256_is_vectorized = 1;
            }
        }
        else
    #endif
    #ifdef HAVE_INTEL_AVX1
        if (IS_INTEL_AVX1(intel_flags)) {
        #ifdef HAVE_INTEL_RORX
            if (IS_INTEL_BMI2(intel_flags)) {
                Transform_Sha256_p = Transform_Sha256_AVX1_RORX;
                Transform_Sha256_Len_p = Transform_Sha256_AVX1_RORX_Len;
                Transform_Sha256_is_vectorized = 1;
            }
            else
        #endif
            {
                Transform_Sha256_p = Transform_Sha256_AVX1;
                Transform_Sha256_Len_p = Transform_Sha256_AVX1_Len;
                Transform_Sha256_is_vectorized = 1;
            }
        }
        else
    #endif
        {
            Transform_Sha256_p = Transform_Sha256;
            Transform_Sha256_Len_p = NULL;
            Transform_Sha256_is_vectorized = 0;
        }

        transform_check = 1;
    }

#if !defined(WOLFSSL_KCAPI_HASH)
    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        int ret = 0;
        if (sha256 == NULL)
            return BAD_FUNC_ARG;

        sha256->heap = heap;
    #ifdef WOLF_CRYPTO_CB
        sha256->devId = devId;
        sha256->devCtx = NULL;
    #endif
    #ifdef WOLFSSL_SMALL_STACK_CACHE
        sha256->W = NULL;
    #endif

        ret = InitSha256(sha256);
        if (ret != 0)
            return ret;

        /* choose best Transform function under this runtime environment */
        Sha256_SetTransform();

    #if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_SHA256)
        ret = wolfAsync_DevCtxInit(&sha256->asyncDev,
                            WOLFSSL_ASYNC_MARKER_SHA256, sha256->heap, devId);
    #else
        (void)devId;
    #endif /* WOLFSSL_ASYNC_CRYPT */

        return ret;
    }
#endif /* !WOLFSSL_KCAPI_HASH */

#elif defined(FREESCALE_LTC_SHA)
    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        (void)heap;
        (void)devId;

        LTC_HASH_Init(LTC_BASE, &sha256->ctx, kLTC_Sha256, NULL, 0);

        return 0;
    }

#elif defined(FREESCALE_MMCAU_SHA)

    #ifdef FREESCALE_MMCAU_CLASSIC_SHA
        #include "cau_api.h"
    #else
        #include "fsl_mmcau.h"
    #endif

    #define XTRANSFORM(S, D)         Transform_Sha256((S),(D))
    #define XTRANSFORM_LEN(S, D, L)  Transform_Sha256_Len((S),(D),(L))

    #ifndef WC_HASH_DATA_ALIGNMENT
        /* these hardware API's require 4 byte (word32) alignment */
        #define WC_HASH_DATA_ALIGNMENT 4
    #endif

    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        int ret = 0;

        (void)heap;
        (void)devId;

        ret = wolfSSL_CryptHwMutexLock();
        if (ret != 0) {
            return ret;
        }

    #ifdef FREESCALE_MMCAU_CLASSIC_SHA
        cau_sha256_initialize_output(sha256->digest);
    #else
        MMCAU_SHA256_InitializeOutput((uint32_t*)sha256->digest);
    #endif
        wolfSSL_CryptHwMutexUnLock();

        sha256->buffLen = 0;
        sha256->loLen   = 0;
        sha256->hiLen   = 0;
    #ifdef WOLFSSL_SMALL_STACK_CACHE
        sha256->W = NULL;
    #endif

        return ret;
    }

    static int Transform_Sha256(wc_Sha256* sha256, const byte* data)
    {
        int ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
    #ifdef FREESCALE_MMCAU_CLASSIC_SHA
            cau_sha256_hash_n((byte*)data, 1, sha256->digest);
    #else
            MMCAU_SHA256_HashN((byte*)data, 1, (uint32_t*)sha256->digest);
    #endif
            wolfSSL_CryptHwMutexUnLock();
        }
        return ret;
    }

    static int Transform_Sha256_Len(wc_Sha256* sha256, const byte* data,
        word32 len)
    {
        int ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
        #if defined(WC_HASH_DATA_ALIGNMENT) && WC_HASH_DATA_ALIGNMENT > 0
            if ((wc_ptr_t)data % WC_HASH_DATA_ALIGNMENT) {
                /* data pointer is NOT aligned,
                 * so copy and perform one block at a time */
                byte* local = (byte*)sha256->buffer;
                while (len >= WC_SHA256_BLOCK_SIZE) {
                    XMEMCPY(local, data, WC_SHA256_BLOCK_SIZE);
                #ifdef FREESCALE_MMCAU_CLASSIC_SHA
                    cau_sha256_hash_n(local, 1, sha256->digest);
                #else
                    MMCAU_SHA256_HashN(local, 1, (uint32_t*)sha256->digest);
                #endif
                    data += WC_SHA256_BLOCK_SIZE;
                    len  -= WC_SHA256_BLOCK_SIZE;
                }
            }
            else
        #endif
            {
    #ifdef FREESCALE_MMCAU_CLASSIC_SHA
            cau_sha256_hash_n((byte*)data, len/WC_SHA256_BLOCK_SIZE,
                sha256->digest);
    #else
            MMCAU_SHA256_HashN((byte*)data, len/WC_SHA256_BLOCK_SIZE,
                (uint32_t*)sha256->digest);
    #endif
            }
            wolfSSL_CryptHwMutexUnLock();
        }
        return ret;
    }

#elif defined(WOLFSSL_PIC32MZ_HASH)
    #include <wolfssl/wolfcrypt/port/pic32/pic32mz-crypt.h>

#elif defined(STM32_HASH_SHA2)

    /* Supports CubeMX HAL or Standard Peripheral Library */

    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        if (sha256 == NULL)
            return BAD_FUNC_ARG;

        (void)devId;
        (void)heap;

        XMEMSET(sha256, 0, sizeof(wc_Sha256));
        wc_Stm32_Hash_Init(&sha256->stmCtx);
        return 0;
    }

    int wc_Sha256Update(wc_Sha256* sha256, const byte* data, word32 len)
    {
        int ret = 0;

        if (sha256 == NULL || (data == NULL && len > 0)) {
            return BAD_FUNC_ARG;
        }

        ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
            ret = wc_Stm32_Hash_Update(&sha256->stmCtx,
                HASH_AlgoSelection_SHA256, data, len, WC_SHA256_BLOCK_SIZE);
            wolfSSL_CryptHwMutexUnLock();
        }
        return ret;
    }

    int wc_Sha256Final(wc_Sha256* sha256, byte* hash)
    {
        int ret = 0;

        if (sha256 == NULL || hash == NULL) {
            return BAD_FUNC_ARG;
        }

        ret = wolfSSL_CryptHwMutexLock();
        if (ret == 0) {
            ret = wc_Stm32_Hash_Final(&sha256->stmCtx,
                HASH_AlgoSelection_SHA256, hash, WC_SHA256_DIGEST_SIZE);
            wolfSSL_CryptHwMutexUnLock();
        }

        (void)wc_InitSha256(sha256); /* reset state */

        return ret;
    }

#elif defined(WOLFSSL_IMX6_CAAM) && !defined(NO_IMX6_CAAM_HASH) && \
    !defined(WOLFSSL_QNX_CAAM)
    /* functions defined in wolfcrypt/src/port/caam/caam_sha256.c */

#elif defined(WOLFSSL_SE050) && defined(WOLFSSL_SE050_HASH)

    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        if (sha256 == NULL) {
            return BAD_FUNC_ARG;
        }
        (void)devId;

        return se050_hash_init(&sha256->se050Ctx, heap);
    }

    int wc_Sha256Update(wc_Sha256* sha256, const byte* data, word32 len)
    {
        return se050_hash_update(&sha256->se050Ctx, data, len);
    }

    int wc_Sha256Final(wc_Sha256* sha256, byte* hash)
    {
        int ret = 0;
        ret = se050_hash_final(&sha256->se050Ctx, hash, WC_SHA256_DIGEST_SIZE,
                               kAlgorithm_SSS_SHA256);
        return ret;
    }
    int wc_Sha256FinalRaw(wc_Sha256* sha256, byte* hash)
    {
        int ret = 0;
        ret = se050_hash_final(&sha256->se050Ctx, hash, WC_SHA256_DIGEST_SIZE,
                               kAlgorithm_SSS_SHA256);
        return ret;
    }

#elif defined(WOLFSSL_AFALG_HASH)
    /* implemented in wolfcrypt/src/port/af_alg/afalg_hash.c */

#elif defined(WOLFSSL_DEVCRYPTO_HASH)
    /* implemented in wolfcrypt/src/port/devcrypto/devcrypt_hash.c */

#elif defined(WOLFSSL_SCE) && !defined(WOLFSSL_SCE_NO_HASH)
    #include "hal_data.h"

    #ifndef WOLFSSL_SCE_SHA256_HANDLE
        #define WOLFSSL_SCE_SHA256_HANDLE g_sce_hash_0
    #endif

    #define WC_SHA256_DIGEST_WORD_SIZE 16
    #define XTRANSFORM(S, D) wc_Sha256SCE_XTRANSFORM((S), (D))
    static int wc_Sha256SCE_XTRANSFORM(wc_Sha256* sha256, const byte* data)
    {
        if (WOLFSSL_SCE_GSCE_HANDLE.p_cfg->endian_flag ==
                CRYPTO_WORD_ENDIAN_LITTLE)
        {
            ByteReverseWords((word32*)data, (word32*)data,
                    WC_SHA256_BLOCK_SIZE);
            ByteReverseWords(sha256->digest, sha256->digest,
                    WC_SHA256_DIGEST_SIZE);
        }

        if (WOLFSSL_SCE_SHA256_HANDLE.p_api->hashUpdate(
                    WOLFSSL_SCE_SHA256_HANDLE.p_ctrl, (word32*)data,
                    WC_SHA256_DIGEST_WORD_SIZE, sha256->digest) != SSP_SUCCESS){
            WOLFSSL_MSG("Unexpected hardware return value");
            return WC_HW_E;
        }

        if (WOLFSSL_SCE_GSCE_HANDLE.p_cfg->endian_flag ==
                CRYPTO_WORD_ENDIAN_LITTLE)
        {
            ByteReverseWords((word32*)data, (word32*)data,
                    WC_SHA256_BLOCK_SIZE);
            ByteReverseWords(sha256->digest, sha256->digest,
                    WC_SHA256_DIGEST_SIZE);
        }

        return 0;
    }


    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        int ret = 0;
        if (sha256 == NULL)
            return BAD_FUNC_ARG;

        sha256->heap = heap;

        ret = InitSha256(sha256);
        if (ret != 0)
            return ret;

        (void)devId;

        return ret;
    }

#elif defined(WOLFSSL_USE_ESP32_CRYPT_HASH_HW)

    /* HW may fail since there's only one, so we still need SW */
    #define NEED_SOFT_SHA256

    /*
    ** An Espressif-specific InitSha256()
    **
    ** soft SHA needs initialization digest, but HW does not.
    */
    static int InitSha256(wc_Sha256* sha256)
    {
        int ret = 0; /* zero = success */

        /* We may or may not need initial digest for HW.
         * Always needed for SW-only. */
        sha256->digest[0] = 0x6A09E667L;
        sha256->digest[1] = 0xBB67AE85L;
        sha256->digest[2] = 0x3C6EF372L;
        sha256->digest[3] = 0xA54FF53AL;
        sha256->digest[4] = 0x510E527FL;
        sha256->digest[5] = 0x9B05688CL;
        sha256->digest[6] = 0x1F83D9ABL;
        sha256->digest[7] = 0x5BE0CD19L;

        sha256->buffLen = 0;
        sha256->loLen   = 0;
        sha256->hiLen   = 0;

#ifndef NO_WOLFSSL_ESP32_CRYPT_HASH_SHA256
        ret = esp_sha_init((WC_ESP32SHA*)&(sha256->ctx), WC_HASH_TYPE_SHA256);
#endif
        return ret;
    }

    /*
    ** An Espressif-specific wolfCrypt InitSha256 external wrapper.
    **
    ** we'll assume this is ALWAYS for a new, uninitialized sha256
    */
    int wc_InitSha256_ex(wc_Sha256* sha256, void* heap, int devId)
    {
        (void)devId;
        if (sha256 == NULL) {
            return BAD_FUNC_ARG;
        }

    #if defined(WOLFSSL_USE_ESP32_CRYPT_HASH_HW) && \
       !defined(NO_WOLFSSL_ESP32_CRYPT_HASH_SHA256)
        /* We know this is a fresh, uninitialized item, so set to INIT */
        if (sha256->ctx.mode != ESP32_SHA_INIT) {
            ESP_LOGV(TAG, "Set ctx mode from prior value: "
                               "%d", sha256->ctx.mode);
        }
        sha256->ctx.mode = ESP32_SHA_INIT;
    #endif

        return InitSha256(sha256);
    }

#elif (defined(WOLFSSL_RENESAS_TSIP_TLS) || \
       defined(WOLFSSL_RENESAS_TSIP_CRYPTONLY)) && \
    !defined(NO_WOLFSSL_RENESAS_TSIP_CRYPT_HASH)

    /* implemented in wolfcrypt/src/port/Renesas/renesas_tsip_sha.c */

#elif (defined(WOLFSSL_RENESAS_SCEPROTECT) || defined(WOLFSSL_RENESAS_RSIP)) \
     && !defined(NO_WOLFSSL_RENESAS_FSPSM_HASH)

    /* implemented in wolfcrypt/src/port/Renesas/renesas_fspsm_sha.c */

#elif defined(WOLFSSL_PSOC6_CRYPTO)

    /* implemented in wolfcrypt/src/port/cypress/psoc6_crypto.c */

#elif defined(WOLFSSL_IMXRT_DCP)
    #include <wolfssl/wolfcrypt/port/nxp/dcp_port.h>
    /* implemented in wolfcrypt/src/port/nxp/dcp_port.c */

#elif defined(WOLFSSL_SILABS_SE_ACCEL)
    /* implemented in wolfcrypt/src/port/silabs/silabs_hash.c */

#elif defined(WOLFSSL_KCAPI_HASH)
    /* implemented in wolfcrypt/src/port/kcapi/kcapi_hash.c */

#elif defined(WOLFSSL_HAVE_PSA) && !defined(WOLFSSL_PSA_NO_HASH)
    /* implemented in wolfcrypt/src/port/psa/psa_hash.c */

#elif defined(WOLFSSL_RENESAS_RX64_HASH)

    /* implemented in wolf
