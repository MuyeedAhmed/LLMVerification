Do not include any additional comments or explanations.

```c
#if defined(FREESCALE_MMCAU_SHA)

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
```
