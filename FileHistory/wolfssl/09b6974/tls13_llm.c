#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/version.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/mem_track.h>
#include <wolfssl/wolfcrypt/memory.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/md2.h>
#include <wolfssl/wolfcrypt/md5.h>
#include <wolfssl/wolfcrypt/md4.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfcrypt/rc2.h>
#include <wolfssl/wolfcrypt/arc4.h>
#include <wolfssl/wolfcrypt/wolfmath.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssl/wolfcrypt/signature.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/des3.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/wc_encrypt.h>
#include <wolfssl/wolfcrypt/cmac.h>
#include <wolfssl/wolfcrypt/siphash.h>
#include <wolfssl/wolfcrypt/poly1305.h>
#include <wolfssl/wolfcrypt/camellia.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/kdf.h>
#include <wolfssl/wolfcrypt/dh.h>
#include <wolfssl/wolfcrypt/dsa.h>
#include <wolfssl/wolfcrypt/srp.h>
#include <wolfssl/wolfcrypt/chacha.h>
#include <wolfssl/wolfcrypt/chacha20_poly1305.h>
#include <wolfssl/wolfcrypt/pwdbased.h>
#include <wolfssl/wolfcrypt/ripemd.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/certs_test.h>
#include "wolfssl/wolfcrypt/port/aria/aria-crypt.h"

// Extracted functions for: dh_ffdhe_test

// --- Function: dh_ffdhe_test ---
static wc_test_ret_t dh_ffdhe_test(WC_RNG *rng, const DhParams* params)
#else
static wc_test_ret_t dh_ffdhe_test(WC_RNG *rng, int name)
#endif
{
    wc_test_ret_t ret;
    word32 privSz, pubSz, privSz2, pubSz2;
#if defined(WOLFSSL_SMALL_STACK) && !defined(WOLFSSL_NO_MALLOC)
    byte   *priv = (byte*)XMALLOC(MAX_DH_PRIV_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    byte   *pub = (byte*)XMALLOC(MAX_DH_KEY_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    byte   *priv2 = (byte*)XMALLOC(MAX_DH_PRIV_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    byte   *pub2 = (byte*)XMALLOC(MAX_DH_KEY_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    byte   *agree = (byte*)XMALLOC(MAX_DH_KEY_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    byte   *agree2 = (byte*)XMALLOC(MAX_DH_KEY_SZ, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    DhKey  *key = (DhKey*)XMALLOC(sizeof(*key), HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    DhKey  *key2 = (DhKey*)XMALLOC(sizeof(*key2), HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
#else
    byte   priv[MAX_DH_PRIV_SZ];
    byte   pub[MAX_DH_KEY_SZ];
    byte   priv2[MAX_DH_PRIV_SZ];
    byte   pub2[MAX_DH_KEY_SZ];
    byte   agree[MAX_DH_KEY_SZ];
    byte   agree2[MAX_DH_KEY_SZ];
    DhKey  key[1];
    DhKey  key2[1];
#endif
    word32 agreeSz = MAX_DH_KEY_SZ;
    word32 agreeSz2 = MAX_DH_KEY_SZ;

#if defined(WOLFSSL_SMALL_STACK) && !defined(WOLFSSL_NO_MALLOC)
    if ((priv == NULL) ||
        (pub == NULL) ||
        (priv2 == NULL) ||
        (pub2 == NULL) ||
        (agree == NULL) ||
        (agree2 == NULL) ||
        (key == NULL) ||
        (key2 == NULL))
        ERROR_OUT(WC_TEST_RET_ENC_NC, done);
#endif

    pubSz = MAX_DH_KEY_SZ;
    pubSz2 = MAX_DH_KEY_SZ;
    #ifdef HAVE_PUBLIC_FFDHE
    privSz = MAX_DH_PRIV_SZ;
    privSz2 = MAX_DH_PRIV_SZ;
    #else
    privSz = wc_DhGetNamedKeyMinSize(name);
    privSz2 = privSz;
    #endif

    XMEMSET(key, 0, sizeof(*key));
    XMEMSET(key2, 0, sizeof(*key2));

    ret = wc_InitDhKey_ex(key, HEAP_HINT, devId);
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);
    ret = wc_InitDhKey_ex(key2, HEAP_HINT, devId);
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

#ifdef HAVE_PUBLIC_FFDHE
    ret = wc_DhSetKey(key, params->p, params->p_len, params->g, params->g_len);
#else
    ret = wc_DhSetNamedKey(key, name);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

#ifdef HAVE_PUBLIC_FFDHE
    ret = wc_DhSetKey(key2, params->p, params->p_len, params->g,
                                                                 params->g_len);
#else
    ret = wc_DhSetNamedKey(key2, name);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    ret = wc_DhGenerateKeyPair(key, rng, priv, &privSz, pub, &pubSz);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    ret = wc_DhGenerateKeyPair(key2, rng, priv2, &privSz2, pub2, &pubSz2);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key2->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    ret = wc_DhAgree(key, agree, &agreeSz, priv, privSz, pub2, pubSz2);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    ret = wc_DhAgree(key2, agree2, &agreeSz2, priv2, privSz2, pub, pubSz);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key2->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    if (agreeSz != agreeSz2 || XMEMCMP(agree, agree2, agreeSz)) {
        ERROR_OUT(WC_TEST_RET_ENC_NC, done);
    }

#if defined(WOLFSSL_HAVE_SP_DH) || defined(USE_FAST_MATH)
    /* Make p even */
    key->p.dp[0] &= (mp_digit)-2;
    if (ret != 0)
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);

    ret = wc_DhGenerateKeyPair(key, rng, priv, &privSz, pub, &pubSz);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != MP_VAL && ret != MP_EXPTMOD_E) {
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);
    }

    ret = wc_DhAgree(key, agree, &agreeSz, priv, privSz, pub2, pubSz2);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &key->asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != MP_VAL && ret != MP_EXPTMOD_E && ret != ASYNC_OP_E) {
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);
    }

#ifndef HAVE_SELFTEST
    ret = wc_DhCheckKeyPair(key, pub, pubSz, priv, privSz);
    if (ret != MP_VAL && ret != MP_EXPTMOD_E && ret != MP_CMP_E &&
            ret != ASYNC_OP_E) {
        ERROR_OUT(WC_TEST_RET_ENC_EC(ret), done);
    }
#endif

    /* Getting here means success - set ret to 0. */
    ret = 0;
#endif

done:

#if defined(WOLFSSL_SMALL_STACK) && !defined(WOLFSSL_NO_MALLOC) && \
    !defined(WC_NO_RNG)
    if (priv)
        XFREE(priv, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (pub)
        XFREE(pub, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (priv2)
        XFREE(priv2, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (pub2)
        XFREE(pub2, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (agree)
        XFREE(agree, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (agree2)
        XFREE(agree2, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (key) {
        wc_FreeDhKey(key);
        XFREE(key, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (key2) {
        wc_FreeDhKey(key2);
        XFREE(key2, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
#else
    wc_FreeDhKey(key);
    wc_FreeDhKey(key2);
#endif

    return ret;
}

