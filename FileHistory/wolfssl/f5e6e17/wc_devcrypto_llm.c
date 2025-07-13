#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/port/devcrypto/wc_devcrypto.h>

// Extracted functions for: wc_DevCryptoCreate

// --- Function: wc_DevCryptoCreate ---
int wc_DevCryptoCreate(WC_CRYPTODEV* ctx, int type, byte* key, word32 keySz)
{
#if defined(CIOCGSESSINFO) && defined(DEBUG_DEVCRYPTO)
    struct session_info_op sesInfo;
#endif

    if (ctx == NULL) {
        return BAD_FUNC_ARG;
    }

    /* sanity check on session type before creating descriptor */
    XMEMSET(ctx, 0, sizeof(WC_CRYPTODEV));

    /* clone the master fd */
    if (ioctl(fd, CRIOGET, &ctx->cfd) != 0) {
        WOLFSSL_MSG("Error cloning fd");
        return WC_DEVCRYPTO_E;
    }

    if (fcntl(ctx->cfd, F_SETFD, 1) == -1) {
        WOLFSSL_MSG("Error setting F_SETFD with fcntl");
        (void)close(ctx->cfd);
        return WC_DEVCRYPTO_E;
    }

    /* set up session */
    switch (type) {
        case CRYPTO_SHA1:
        case CRYPTO_SHA2_256:
            ctx->sess.mac = type;
            break;

    #ifndef NO_AES
        case CRYPTO_AES_CTR:
        case CRYPTO_AES_ECB:
        case CRYPTO_AES_GCM:
        case CRYPTO_AES_CBC:
            ctx->sess.cipher = type;
            ctx->sess.key    = (void*)key;
            ctx->sess.keylen = keySz;
            break;
    #endif

        case CRYPTO_MD5_HMAC:
        case CRYPTO_SHA1_HMAC:
        case CRYPTO_SHA2_256_HMAC:
        case CRYPTO_SHA2_384_HMAC:
        case CRYPTO_SHA2_512_HMAC:
            ctx->sess.cipher = 0;
            ctx->sess.mac    = type;
            ctx->sess.mackey    = (byte*)key;
            ctx->sess.mackeylen = keySz;
            break;

    #if defined(WOLFSSL_DEVCRYPTO_ECDSA)
    #endif /* WOLFSSL_DEVCRYPTO_ECDSA */

    #if defined(WOLFSSL_DEVCRYPTO_RSA)
        case CRYPTO_ASYM_RSA_KEYGEN:
        case CRYPTO_ASYM_RSA_PRIVATE:
        case CRYPTO_ASYM_RSA_PUBLIC:
            ctx->sess.acipher = type;
            break;
    #endif

    #if defined(WOLFSSL_DEVCRYPTO_ECDSA)
        case CRYPTO_ASYM_ECDSA_SIGN:
        case CRYPTO_ASYM_ECDSA_VERIFY:
        case CRYPTO_ASYM_ECC_KEYGEN:
        case CRYPTO_ASYM_ECC_ECDH:
            ctx->sess.acipher = type;
            break;
    #endif

    #if defined(WOLFSSL_DEVCRYPTO_CURVE25519)
        case CRYPTO_ASYM_MUL_MOD:
            ctx->sess.acipher = type;
            break;
    #endif /* WOLFSSL_DEVCRYPTO_CURVE25519 */

        default:
            WOLFSSL_MSG("Unknown / Unimplemented algorithm type");
            (void)close(ctx->cfd);
            return BAD_FUNC_ARG;
    }


    if (ioctl(ctx->cfd, CIOCGSESSION, &ctx->sess)) {
    #if defined(DEBUG_DEVCRYPTO)
        perror("CIOGSESSION error ");
    #endif
        (void)close(ctx->cfd);
        WOLFSSL_MSG("Error starting cryptodev session");
        return WC_DEVCRYPTO_E;
    }

#if defined(CIOCGSESSINFO) && defined(DEBUG_DEVCRYPTO)
    sesInfo.ses = ctx->sess.ses;
    if (ioctl(ctx->cfd, CIOCGSESSINFO, &sesInfo)) {
        (void)close(ctx->cfd);
        WOLFSSL_MSG("Error getting session info");
        return WC_DEVCRYPTO_E;
    }
    printf("Using %s with driver %s\n", sesInfo.hash_info.cra_name,
        sesInfo.hash_info.cra_driver_name);
#endif
    (void)key;
    (void)keySz;

    return 0;
}

// --- Function: XMEMSET ---
// --- Function: ioctl ---
// --- Function: WOLFSSL_MSG ---
// --- Function: fcntl ---
// --- Function: close ---
// --- Function: perror ---
// --- Function: printf ---
