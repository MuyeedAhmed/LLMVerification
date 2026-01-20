Do not include anything else.

----- BEGIN MODIFIED.c -----
/* x509_str.c
 *
 * Copyright (C) 2006-2024 wolfSSL Inc.
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


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#if !defined(WOLFSSL_X509_STORE_INCLUDED)
    #ifndef WOLFSSL_IGNORE_FILE_WARN
        #warning x509_str.c does not need to be compiled separately from ssl.c
    #endif
#else

#ifndef WOLFCRYPT_ONLY

#ifndef NO_CERTS

#ifdef OPENSSL_EXTRA
static int X509StoreGetIssuerEx(WOLFSSL_X509 **issuer,
                            WOLFSSL_STACK *certs, WOLFSSL_X509 *x);
static int X509StoreAddCa(WOLFSSL_X509_STORE* store,
                                          WOLFSSL_X509* x509, int type);
#endif

/* Based on OpenSSL default max depth */
#ifndef WOLFSSL_X509_STORE_DEFAULT_MAX_DEPTH
#define WOLFSSL_X509_STORE_DEFAULT_MAX_DEPTH 100
#endif

/******************************************************************************
 * START OF X509_STORE_CTX APIs
 *****************************************************************************/

/* This API is necessary outside of OPENSSL_EXTRA because it is used in
 * SetupStoreCtxCallback */
WOLFSSL_X509_STORE_CTX* wolfSSL_X509_STORE_CTX_new_ex(void* heap)
{
    WOLFSSL_X509_STORE_CTX* ctx;
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_new_ex");

    ctx = (WOLFSSL_X509_STORE_CTX*)XMALLOC(sizeof(WOLFSSL_X509_STORE_CTX), heap,
                                    DYNAMIC_TYPE_X509_CTX);
    if (ctx != NULL) {
        XMEMSET(ctx, 0, sizeof(WOLFSSL_X509_STORE_CTX));
        ctx->heap = heap;
#ifdef OPENSSL_EXTRA
        if ((ctx->owned = wolfSSL_sk_X509_new_null()) == NULL) {
            XFREE(ctx, heap, DYNAMIC_TYPE_X509_CTX);
            ctx = NULL;
        }
        if (ctx != NULL &&
            wolfSSL_X509_STORE_CTX_init(ctx, NULL, NULL, NULL) !=
                WOLFSSL_SUCCESS) {
            wolfSSL_X509_STORE_CTX_free(ctx);
            ctx = NULL;
        }
#endif
    }

    return ctx;
}

/* This API is necessary outside of OPENSSL_EXTRA because it is used in
 * SetupStoreCtxCallback */
/* free's extra data */
void wolfSSL_X509_STORE_CTX_free(WOLFSSL_X509_STORE_CTX* ctx)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_free");
    if (ctx != NULL) {
#ifdef HAVE_EX_DATA_CLEANUP_HOOKS
        wolfSSL_CRYPTO_cleanup_ex_data(&ctx->ex_data);
#endif

#ifdef OPENSSL_EXTRA
        XFREE(ctx->param, ctx->heap, DYNAMIC_TYPE_OPENSSL);
        ctx->param = NULL;

        if (ctx->chain != NULL) {
            wolfSSL_sk_X509_free(ctx->chain);
        }
        if (ctx->owned != NULL) {
            wolfSSL_sk_X509_pop_free(ctx->owned, NULL);
        }

        if (ctx->current_issuer != NULL) {
            wolfSSL_X509_free(ctx->current_issuer);
        }
#endif

        XFREE(ctx, ctx->heap, DYNAMIC_TYPE_X509_CTX);
    }
}

#ifdef OPENSSL_EXTRA

#if ((defined(SESSION_CERTS) && !defined(WOLFSSL_QT)) || \
      defined(WOLFSSL_SIGNER_DER_CERT))

/**
 * Find the issuing cert of the input cert. On a self-signed cert this
 * function will return an error.
 * @param issuer The issuer x509 struct is returned here
 * @param cm     The cert manager that is queried for the issuer
 * @param x      This cert's issuer will be queried in cm
 * @return       WOLFSSL_SUCCESS on success
 *               WOLFSSL_FAILURE on error
 */
static int x509GetIssuerFromCM(WOLFSSL_X509 **issuer, WOLFSSL_CERT_MANAGER* cm,
        WOLFSSL_X509 *x)
{
    Signer* ca = NULL;
#ifdef WOLFSSL_SMALL_STACK
    DecodedCert* cert = NULL;
#else
    DecodedCert  cert[1];
#endif

    if (cm == NULL || x == NULL || x->derCert == NULL) {
        WOLFSSL_MSG("No cert DER buffer or NULL cm. Defining "
                    "WOLFSSL_SIGNER_DER_CERT could solve the issue");
        return WOLFSSL_FAILURE;
    }

#ifdef WOLFSSL_SMALL_STACK
    cert = (DecodedCert*)XMALLOC(sizeof(DecodedCert), NULL, DYNAMIC_TYPE_DCERT);
    if (cert == NULL)
        return WOLFSSL_FAILURE;
#endif

    /* Use existing CA retrieval APIs that use DecodedCert. */
    InitDecodedCert(cert, x->derCert->buffer, x->derCert->length, cm->heap);
    if (ParseCertRelative(cert, CERT_TYPE, 0, NULL, NULL) == 0
            && !cert->selfSigned) {
    #ifndef NO_SKID
        if (cert->extAuthKeyIdSet)
            ca = GetCA(cm, cert->extAuthKeyId);
        if (ca == NULL)
            ca = GetCAByName(cm, cert->issuerHash);
    #else /* NO_SKID */
        ca = GetCA(cm, cert->issuerHash);
    #endif /* NO SKID */
    }
    FreeDecodedCert(cert);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(cert, NULL, DYNAMIC_TYPE_DCERT);
#endif

    if (ca == NULL)
        return WOLFSSL_FAILURE;

#ifdef WOLFSSL_SIGNER_DER_CERT
    /* populate issuer with Signer DER */
    if (wolfSSL_X509_d2i_ex(issuer, ca->derCert->buffer,
            ca->derCert->length, cm->heap) == NULL)
        return WOLFSSL_FAILURE;
#else
    /* Create an empty certificate as CA doesn't have a certificate. */
    *issuer = (WOLFSSL_X509 *)XMALLOC(sizeof(WOLFSSL_X509), 0,
        DYNAMIC_TYPE_OPENSSL);
    if (*issuer == NULL)
        return WOLFSSL_FAILURE;

    InitX509((*issuer), 1, NULL);
#endif

    return WOLFSSL_SUCCESS;
}
#endif /* SESSION_CERTS || WOLFSSL_SIGNER_DER_CERT */

WOLFSSL_X509_STORE_CTX* wolfSSL_X509_STORE_CTX_new(void)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_new");
    return wolfSSL_X509_STORE_CTX_new_ex(NULL);
}

int wolfSSL_X509_STORE_CTX_init(WOLFSSL_X509_STORE_CTX* ctx,
     WOLFSSL_X509_STORE* store, WOLFSSL_X509* x509,
     WOLF_STACK_OF(WOLFSSL_X509)* sk)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_init");

    if (ctx != NULL) {
        ctx->store = store;
        #ifndef WOLFSSL_X509_STORE_CERTS
        ctx->current_cert = x509;
        #else
        if(x509 != NULL){
            ctx->current_cert = wolfSSL_X509_d2i_ex(NULL,
                    x509->derCert->buffer,
                    x509->derCert->length,
                    x509->heap);
            if(ctx->current_cert == NULL)
                return WOLFSSL_FAILURE;
        } else
            ctx->current_cert = NULL;
        #endif

        ctx->ctxIntermediates = sk;
        if (ctx->chain != NULL) {
            wolfSSL_sk_X509_free(ctx->chain);
            ctx->chain = NULL;
        }
#ifdef SESSION_CERTS
        ctx->sesChain = NULL;
#endif
        ctx->domain = NULL;
#ifdef HAVE_EX_DATA
        XMEMSET(&ctx->ex_data, 0, sizeof(ctx->ex_data));
#endif
        ctx->userCtx = NULL;
        ctx->error = 0;
        ctx->error_depth = 0;
        ctx->discardSessionCerts = 0;

        if (ctx->param == NULL) {
            ctx->param = (WOLFSSL_X509_VERIFY_PARAM*)XMALLOC(
                           sizeof(WOLFSSL_X509_VERIFY_PARAM),
                           ctx->heap, DYNAMIC_TYPE_OPENSSL);
            if (ctx->param == NULL){
                WOLFSSL_MSG("wolfSSL_X509_STORE_CTX_init failed");
                return WOLFSSL_FAILURE;
            }
            XMEMSET(ctx->param, 0, sizeof(*ctx->param));
        }

        return WOLFSSL_SUCCESS;
    }
    return WOLFSSL_FAILURE;
}

/* Its recommended to use a full free -> init cycle of all the objects
 * because wolfSSL_X509_STORE_CTX_init may modify the store too which doesn't
 * get reset here. */
void wolfSSL_X509_STORE_CTX_cleanup(WOLFSSL_X509_STORE_CTX* ctx)
{
    if (ctx != NULL) {

        XFREE(ctx->param, ctx->heap, DYNAMIC_TYPE_OPENSSL);
        ctx->param = NULL;

        wolfSSL_X509_STORE_CTX_init(ctx, NULL, NULL, NULL);
    }
}


void wolfSSL_X509_STORE_CTX_trusted_stack(WOLFSSL_X509_STORE_CTX *ctx,
                                          WOLF_STACK_OF(WOLFSSL_X509) *sk)
{
    if (ctx != NULL) {
        ctx->setTrustedSk = sk;
    }
}


/* Returns corresponding X509 error from internal ASN error <e> */
int GetX509Error(int e)
{
    switch (e) {
        case WC_NO_ERR_TRACE(ASN_BEFORE_DATE_E):
            return WOLFSSL_X509_V_ERR_CERT_NOT_YET_VALID;
        case WC_NO_ERR_TRACE(ASN_AFTER_DATE_E):
            return WOLFSSL_X509_V_ERR_CERT_HAS_EXPIRED;
        case WC_NO_ERR_TRACE(ASN_NO_SIGNER_E):
            /* get issuer error if no CA found locally */
            return WOLFSSL_X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY;
        case WC_NO_ERR_TRACE(ASN_SELF_SIGNED_E):
            return WOLFSSL_X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT;
        case WC_NO_ERR_TRACE(ASN_PATHLEN_INV_E):
        case WC_NO_ERR_TRACE(ASN_PATHLEN_SIZE_E):
            return WOLFSSL_X509_V_ERR_PATH_LENGTH_EXCEEDED;
        case WC_NO_ERR_TRACE(ASN_SIG_OID_E):
        case WC_NO_ERR_TRACE(ASN_SIG_CONFIRM_E):
        case WC_NO_ERR_TRACE(ASN_SIG_HASH_E):
        case WC_NO_ERR_TRACE(ASN_SIG_KEY_E):
            return WOLFSSL_X509_V_ERR_CERT_SIGNATURE_FAILURE;
        /* We can't disambiguate if its the before or after date that caused
         * the error. Assume expired. */
        case WC_NO_ERR_TRACE(CRL_CERT_DATE_ERR):
            return WOLFSSL_X509_V_ERR_CRL_HAS_EXPIRED;
        case WC_NO_ERR_TRACE(CRL_CERT_REVOKED):
            return WOLFSSL_X509_V_ERR_CERT_REVOKED;
        case WC_NO_ERR_TRACE(CRL_MISSING):
            return WOLFSSL_X509_V_ERR_UNABLE_TO_GET_CRL;
        case 0:
        case 1:
            return 0;
        default:
#ifdef HAVE_WOLFSSL_MSG_EX
            WOLFSSL_MSG_EX("Error not configured or implemented yet: %d", e);
#else
            WOLFSSL_MSG("Error not configured or implemented yet");
#endif
            return e;
    }
}

static void SetupStoreCtxError_ex(WOLFSSL_X509_STORE_CTX* ctx, int ret,
                                                                    int depth)
{
    int error = GetX509Error(ret);

    wolfSSL_X509_STORE_CTX_set_error(ctx, error);
    wolfSSL_X509_STORE_CTX_set_error_depth(ctx, depth);
}

static void SetupStoreCtxError(WOLFSSL_X509_STORE_CTX* ctx, int ret)
{
    int depth = 0;

    /* Set error depth */
    if (ctx->chain)
        depth = (int)ctx->chain->num;

    SetupStoreCtxError_ex(ctx, ret, depth);
}

static int X509StoreVerifyCert(WOLFSSL_X509_STORE_CTX* ctx)
{
    int ret = WC_NO_ERR_TRACE(WOLFSSL_FAILURE);
    WOLFSSL_ENTER("X509StoreVerifyCert");

    if (ctx->current_cert != NULL && ctx->current_cert->derCert != NULL) {
        ret = wolfSSL_CertManagerVerifyBuffer(ctx->store->cm,
                    ctx->current_cert->derCert->buffer,
                    ctx->current_cert->derCert->length,
                    WOLFSSL_FILETYPE_ASN1);
        SetupStoreCtxError(ctx, ret);
    #if defined(OPENSSL_ALL) || defined(WOLFSSL_QT)
        if (ctx->store->verify_cb)
            ret = ctx->store->verify_cb(ret >= 0 ? 1 : 0, ctx) == 1 ?
                                                        WOLFSSL_SUCCESS : ret;
    #endif

    #ifndef NO_ASN_TIME
        if (ret != WC_NO_ERR_TRACE(ASN_BEFORE_DATE_E) &&
            ret != WC_NO_ERR_TRACE(ASN_AFTER_DATE_E)) {
            /* wolfSSL_CertManagerVerifyBuffer only returns ASN_AFTER_DATE_E or
             * ASN_BEFORE_DATE_E if there are no additional errors found in the
             * cert. Therefore, check if the cert is expired or not yet valid
             * in order to return the correct expected error. */
            byte *afterDate = ctx->current_cert->notAfter.data;
            byte *beforeDate = ctx->current_cert->notBefore.data;

            if (XVALIDATE_DATE(afterDate,
                    (byte)ctx->current_cert->notAfter.type, ASN_AFTER) < 1) {
                ret = ASN_AFTER_DATE_E;
            }
            else if (XVALIDATE_DATE(beforeDate,
                    (byte)ctx->current_cert->notBefore.type, ASN_BEFORE) < 1) {
                ret = ASN_BEFORE_DATE_E;
            }
            SetupStoreCtxError(ctx, ret);
        #if defined(OPENSSL_ALL) || defined(WOLFSSL_QT)
            if (ctx->store->verify_cb)
                ret = ctx->store->verify_cb(ret >= 0 ? 1 : 0,
                                            ctx) == 1 ? WOLFSSL_SUCCESS : -1;
        #endif
        }
    #endif
    }

    return ret;
}

static int addAllButSelfSigned(WOLF_STACK_OF(WOLFSSL_X509)*to,
                               WOLF_STACK_OF(WOLFSSL_X509)*from, int *numAdded)
{
    int ret = WOLFSSL_SUCCESS;
    int i = 0;
    int cnt = 0;
    WOLFSSL_X509 *x = NULL;

    for (i = 0; i < wolfSSL_sk_X509_num(from); i++) {
        x = wolfSSL_sk_X509_value(from, i);
        if (wolfSSL_X509_NAME_cmp(&x->issuer, &x->subject) != 0) {
            if (wolfSSL_sk_X509_push(to, x) <= 0) {
                ret = WOLFSSL_FAILURE;
                goto exit;
            }
            cnt++;
        }
    }

exit:
    if (numAdded != NULL) {
        *numAdded = cnt;
    }
    return ret;
}

/* Verifies certificate chain using WOLFSSL_X509_STORE_CTX
 * returns 0 on success or < 0 on failure.
 */
int wolfSSL_X509_verify_cert(WOLFSSL_X509_STORE_CTX* ctx)
{
    int ret = WC_NO_ERR_TRACE(WOLFSSL_FAILURE);
    int done = 0;
    int added = 0;
    int i = 0;
    int numInterAdd = 0;
    int depth = 0;
    WOLFSSL_X509 *issuer = NULL;
    WOLFSSL_X509 *orig = NULL;
    WOLF_STACK_OF(WOLFSSL_X509)* certs = NULL;
    WOLF_STACK_OF(WOLFSSL_X509)* certsToUse = NULL;
    WOLFSSL_ENTER("wolfSSL_X509_verify_cert");

    if (ctx == NULL || ctx->store == NULL || ctx->store->cm == NULL
         || ctx->current_cert == NULL || ctx->current_cert->derCert == NULL) {
        return WOLFSSL_FATAL_ERROR;
    }

    certs = ctx->store->certs;
    if (ctx->setTrustedSk != NULL) {
        certs = ctx->setTrustedSk;
    }

    if (certs == NULL &&
        wolfSSL_sk_X509_num(ctx->ctxIntermediates) > 0) {
        certsToUse = wolfSSL_sk_X509_new_null();
        ret = addAllButSelfSigned(certsToUse, ctx->ctxIntermediates, NULL);
    }
    else {
        /* Add the intermediates provided on init to the list of untrusted
         * intermediates to be used */
        ret = addAllButSelfSigned(certs, ctx->ctxIntermediates, &numInterAdd);
    }
    if (ret != WOLFSSL_SUCCESS) {
        goto exit;
    }

    if (ctx->chain != NULL) {
        wolfSSL_sk_X509_free(ctx->chain);
    }
    ctx->chain = wolfSSL_sk_X509_new_null();

    if (ctx->depth > 0) {
        depth = ctx->depth + 1;
    }
    else {
        depth = WOLFSSL_X509_STORE_DEFAULT_MAX_DEPTH + 1;
    }

    orig = ctx->current_cert;
    while(done == 0 && depth > 0) {
        issuer = NULL;

        /* Try to find an untrusted issuer first */
        ret = X509StoreGetIssuerEx(&issuer, certs,
                                               ctx->current_cert);
        if (ret == WOLFSSL_SUCCESS) {
            if (ctx->current_cert == issuer) {
                wolfSSL_sk_X509_push(ctx->chain, ctx->current_cert);
                break;
            }

            /* We found our issuer in the non-trusted list, add it
             * to the CM and verify the current cert against it */
        #if defined(OPENSSL_ALL) || defined(WOLFSSL_QT)
            /* OpenSSL doesn't allow the cert as CA if it is not CA:TRUE for
             * intermediate certs.
             */
            if (!issuer->isCa) {
                /* error depth is current depth + 1 */
                SetupStoreCtxError_ex(ctx, X509_V_ERR_INVALID_CA,
                                (ctx->chain) ? (int)(ctx->chain->num + 1) : 1);
                if (ctx->store->verify_cb) {
                    ret = ctx->store->verify_cb(0, ctx);
                    if (ret != WOLFSSL_SUCCESS) {
                        goto exit;
                    }
                }
            } else {
        #endif
            ret = X509StoreAddCa(ctx->store, issuer,
                                            WOLFSSL_TEMP_CA);
            if (ret != WOLFSSL_SUCCESS) {
                goto exit;
            }
            added = 1;
            ret = X509StoreVerifyCert(ctx);
            if (ret != WOLFSSL_SUCCESS) {
                goto exit;
            }
            /* Add it to the current chain and look at the issuer cert next */
            wolfSSL_sk_X509_push(ctx->chain, ctx->current_cert);
        #if defined(OPENSSL_ALL) || defined(WOLFSSL_QT)
            }
        #endif
            ctx->current_cert = issuer;
        }
        else if (ret == WC_NO_ERR_TRACE(WOLFSSL_FAILURE)) {
            /* Could not find in untrusted list, only place left is
             * a trusted CA in the CM */
            ret = X509StoreVerifyCert(ctx);
            if (ret != WOLFSSL_SUCCESS) {
                if (((ctx->flags & WOLFSSL_PARTIAL_CHAIN) ||
                     (ctx->store->param->flags & WOLFSSL_PARTIAL_CHAIN)) &&
                    (added == 1)) {
                    wolfSSL_sk_X509_push(ctx->chain, ctx->current_cert);
                    ret = WOLFSSL_SUCCESS;
                }
                goto exit;
            }

            /* Cert verified, finish building the chain */
            wolfSSL_sk_X509_push(ctx->chain, ctx->current_cert);
            issuer = NULL;
    #ifdef WOLFSSL_SIGNER_DER_CERT
            x509GetIssuerFromCM(&issuer, ctx->store->cm, ctx->current_cert);
            if (issuer != NULL && ctx->owned != NULL) {
                wolfSSL_sk_X509_push(ctx->owned, issuer);
            }
    #else
            if (ctx->setTrustedSk == NULL) {
                X509StoreGetIssuerEx(&issuer,
                    ctx->store->trusted, ctx->current_cert);
            }
            else {
                X509StoreGetIssuerEx(&issuer,
                    ctx->setTrustedSk, ctx->current_cert);
            }
    #endif
            if (issuer != NULL) {
                wolfSSL_sk_X509_push(ctx->chain, issuer);
            }

            done = 1;
        }
        else {
            goto exit;
        }

        depth--;
    }

exit:
    /* Remove additional intermediates from init from the store */
    if (ctx != NULL && numInterAdd > 0) {
        for (i = 0; i < numInterAdd; i++) {
            wolfSSL_sk_X509_pop(ctx->store->certs);
        }
    }
    /* Remove intermediates that were added to CM */
    if (ctx != NULL) {
        if (ctx->store != NULL) {
            if (added == 1) {
                wolfSSL_CertManagerUnloadTempIntermediateCerts(ctx->store->cm);
            }
        }
        if (orig != NULL) {
            ctx->current_cert = orig;
        }
    }
    if (certsToUse != NULL) {
        wolfSSL_sk_X509_free(certsToUse);
    }

    return ret == WOLFSSL_SUCCESS ? WOLFSSL_SUCCESS : WOLFSSL_FAILURE;
}

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(WOLFSSL_WPAS_SMALL)
    WOLFSSL_X509* wolfSSL_X509_STORE_CTX_get_current_cert(
                                                WOLFSSL_X509_STORE_CTX* ctx)
    {
        WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get_current_cert");
        if (ctx)
            return ctx->current_cert;
        return NULL;
    }


    int wolfSSL_X509_STORE_CTX_get_error(WOLFSSL_X509_STORE_CTX* ctx)
    {
        WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get_error");
        if (ctx != NULL)
            return ctx->error;
        return 0;
    }


    int wolfSSL_X509_STORE_CTX_get_error_depth(WOLFSSL_X509_STORE_CTX* ctx)
    {
        WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get_error_depth");
        if(ctx)
            return ctx->error_depth;
        return WOLFSSL_FATAL_ERROR;
    }

/* get X509_STORE_CTX ex_data, max idx is MAX_EX_DATA */
void* wolfSSL_X509_STORE_CTX_get_ex_data(WOLFSSL_X509_STORE_CTX* ctx, int idx)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get_ex_data");
#ifdef HAVE_EX_DATA
    if (ctx != NULL) {
        return wolfSSL_CRYPTO_get_ex_data(&ctx->ex_data, idx);
    }
#else
    (void)ctx;
    (void)idx;
#endif
    return NULL;
}
#endif /* OPENSSL_EXTRA || WOLFSSL_WPAS_SMALL */

#ifdef OPENSSL_EXTRA
    void wolfSSL_X509_STORE_CTX_set_verify_cb(WOLFSSL_X509_STORE_CTX *ctx,
                                  WOLFSSL_X509_STORE_CTX_verify_cb verify_cb)
    {
        WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_verify_cb");
        if(ctx == NULL)
            return;
        ctx->verify_cb = verify_cb;
    }

/* Gets pointer to X509_STORE that was used to create context.
 *
 * Return valid pointer on success, NULL if ctx was NULL or not initialized
 */
WOLFSSL_X509_STORE* wolfSSL_X509_STORE_CTX_get0_store(
        WOLFSSL_X509_STORE_CTX* ctx)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get0_store");

    if (ctx == NULL)
        return NULL;

    return ctx->store;
}

WOLFSSL_X509* wolfSSL_X509_STORE_CTX_get0_cert(WOLFSSL_X509_STORE_CTX* ctx)
{
    if (ctx == NULL)
        return NULL;

    return ctx->current_cert;
}

void wolfSSL_X509_STORE_CTX_set_time(WOLFSSL_X509_STORE_CTX* ctx,
                                    unsigned long flags,
                                    time_t t)
{
    (void)flags;

    if (ctx == NULL || ctx->param == NULL)
        return;

    ctx->param->check_time = t;
    ctx->param->flags |= WOLFSSL_USE_CHECK_TIME;
}

#if defined(WOLFSSL_QT) || defined(OPENSSL_ALL)
#ifndef NO_WOLFSSL_STUB
int wolfSSL_X509_STORE_CTX_set_purpose(WOLFSSL_X509_STORE_CTX *ctx,
                                       int purpose)
{
    (void)ctx;
    (void)purpose;
    WOLFSSL_STUB("wolfSSL_X509_STORE_CTX_set_purpose (not implemented)");
    return 0;
}
#endif /* !NO_WOLFSSL_STUB */

#endif /* WOLFSSL_QT || OPENSSL_ALL */
#endif /* OPENSSL_EXTRA */

#ifdef OPENSSL_EXTRA

void wolfSSL_X509_STORE_CTX_set_flags(WOLFSSL_X509_STORE_CTX *ctx,
        unsigned long flags)
{
    if ((ctx != NULL) && (flags & WOLFSSL_PARTIAL_CHAIN)){
        ctx->flags |= WOLFSSL_PARTIAL_CHAIN;
    }
}

/* set X509_STORE_CTX ex_data, max idx is MAX_EX_DATA. Return WOLFSSL_SUCCESS
 * on success, WOLFSSL_FAILURE on error. */
int wolfSSL_X509_STORE_CTX_set_ex_data(WOLFSSL_X509_STORE_CTX* ctx, int idx,
                                       void *data)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_ex_data");
#ifdef HAVE_EX_DATA
    if (ctx != NULL)
    {
        return wolfSSL_CRYPTO_set_ex_data(&ctx->ex_data, idx, data);
    }
#else
    (void)ctx;
    (void)idx;
    (void)data;
#endif
    return WOLFSSL_FAILURE;
}

#ifdef HAVE_EX_DATA_CLEANUP_HOOKS
/* set X509_STORE_CTX ex_data, max idx is MAX_EX_DATA. Return WOLFSSL_SUCCESS
 * on success, WOLFSSL_FAILURE on error. */
int wolfSSL_X509_STORE_CTX_set_ex_data_with_cleanup(
    WOLFSSL_X509_STORE_CTX* ctx,
    int idx,
    void *data,
    wolfSSL_ex_data_cleanup_routine_t cleanup_routine)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_ex_data_with_cleanup");
    if (ctx != NULL)
    {
        return wolfSSL_CRYPTO_set_ex_data_with_cleanup(&ctx->ex_data, idx,
                                                        data, cleanup_routine);
    }
    return WOLFSSL_FAILURE;
}
#endif /* HAVE_EX_DATA_CLEANUP_HOOKS */

#if defined(WOLFSSL_APACHE_HTTPD) || defined(OPENSSL_ALL)
void wolfSSL_X509_STORE_CTX_set_depth(WOLFSSL_X509_STORE_CTX* ctx, int depth)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_depth");
    if (ctx)
        ctx->depth = depth;
}
#endif

WOLFSSL_X509* wolfSSL_X509_STORE_CTX_get0_current_issuer(
        WOLFSSL_X509_STORE_CTX* ctx)
{
    WOLFSSL_STACK* node;
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get0_current_issuer");

    if (ctx == NULL)
        return NULL;

    /* get0 only checks currently built chain */
    if (ctx->chain != NULL) {
        for (node = ctx->chain; node != NULL; node = node->next) {
            if (wolfSSL_X509_check_issued(node->data.x509,
                                          ctx->current_cert) ==
                                                WOLFSSL_X509_V_OK) {
                return node->data.x509;
            }
        }
    }

    return NULL;
}

/* Set an error stat in the X509 STORE CTX
 *
 */
void wolfSSL_X509_STORE_CTX_set_error(WOLFSSL_X509_STORE_CTX* ctx, int er)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_error");

    if (ctx != NULL) {
        ctx->error = er;
    }
}

/* Set the error depth in the X509 STORE CTX */
void wolfSSL_X509_STORE_CTX_set_error_depth(WOLFSSL_X509_STORE_CTX* ctx,
                                                                    int depth)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_set_error_depth");

    if (ctx != NULL) {
        ctx->error_depth = depth;
    }
}

WOLFSSL_STACK* wolfSSL_X509_STORE_CTX_get_chain(WOLFSSL_X509_STORE_CTX* ctx)
{
    WOLFSSL_ENTER("wolfSSL_X509_STORE_CTX_get_chain");

    if (ctx == NULL) {
        return NULL;
    }

#ifdef SESSION_CERTS
    /* if chain is null but sesChain is available then populate stack */
    if (ctx->chain == NULL && ctx->sesChain != NULL) {
        int i;
        int error = 0;
        WOLFSSL_X509_CHAIN* c = ctx->sesChain;
        WOLFSSL_STACK*     sk = wolfSSL_sk_new_node(ctx->heap);

        if (sk == NULL)
            return NULL;

#if defined(WOLFSSL_NGINX) || defined(WOLFSSL_HAPROXY) || \
    defined(OPENSSL_EXTRA)
        /* add CA used to verify top of chain to the list */
        if (c->count > 0) {
            WOLFSSL_X509* x509 = wolfSSL_get_chain_X509(c, c->count - 1);
            WOLFSSL_X509* issuer = NULL;
            if (x509 != NULL) {
                if (wolfSSL_X509_STORE_CTX_get1_issuer(&issuer, ctx, x509)
                        == WOLFSSL_SUCCESS) {
                    /* check that the certificate being looked up is not self
                     * signed and that a issuer was found */
                    if (issuer != NULL && wolfSSL_X509_NAME_cmp(&x509->issuer,
                                &x509->subject) != 0) {
                        if (wolfSSL_sk_X509_push(sk, issuer) <= 0) {
                            WOLFSSL_MSG("Unable to load CA x509 into stack");
                            error = 1;
                        }
                    }
                    else {
                        WOLFSSL_MSG("Certificate is self signed");
                        wolfSSL_X509_free(issuer);
                    }
                }
                else {
                    WOLFSSL_MSG("Could not find CA for certificate");
                }
            }
            wolfSSL_X509_free(x509);
            if (error) {
                wolfSSL_sk_X509_pop_free(sk, NULL);
                wolfSSL_X509_free(issuer);
                return NULL;
            }
        }
#endif

        for (i = c->count - 1; i >= 0; i--) {
            WOLFSSL_X509* x509 = wolfSSL_get_chain_X509(c, i);

            if (x509 == NULL) {
                WOLFSSL_MSG("Unable to get x509 from chain");
                error = 1;
                break;
            }

            if (wolfSSL_sk_X509_push(sk, x509) <= 0) {
                WOLFSSL_MSG("Unable to load x509 into stack");
                wolfSSL_X509_free(x509);
                error = 1;
                break;
            }
        }
        if (error) {
            wolfSSL_sk_X509_pop_free(sk, NULL);
            return NULL;
        }
        ctx->chain = sk;
    }
#endif /* SESSION_CERTS */

    return ctx->chain;
}

/* like X509_STORE_CTX_get_chain(), but return a copy with data reference
   counts increased */
WOLFSSL_STACK* wolfSSL_X509_STORE_CTX_get1_chain(WOLFSSL_X509_STORE_CTX* ctx)
{
    WOLFSSL_STACK* ref;

    if (ctx == NULL) {
        return NULL;
    }

    /* get chain in ctx */
    ref = wolfSSL_X509_STORE_CTX_get_chain(ctx);
    if (ref == NULL) {
        return ref;
    }

    /* create duplicate of ctx chain */
    return wolfSSL_sk_dup(ref);
}

#ifndef NO_WOLFSSL_STUB
WOLFSSL_X509_STORE_CTX *wolfSSL_X509_STORE_CTX_get0_parent_ctx(
                                                   WOLFSSL_X509_STORE_CTX *ctx)
{
    (void)ctx;
    WOLFSSL_STUB("wolfSSL_X509_STORE_CTX_get0_parent_ctx");
    return NULL;
}

int wolfSSL_X509_STORE_get_by_subject(WOLFSSL_X509_STORE_CTX* ctx, int idx,
                            WOLFSSL_X509_NAME* name, WOLFSSL_X509_OBJECT* obj)
{
    (void)ctx;
    (void)idx;
    (void)name;
    (void)obj;
    WOLFSSL_STUB("X509_STORE_get_by_subject");
    return 0;
}
#endif

WOLFSSL_X509_VERIFY_PARAM *wolfSSL_X509_STORE_CTX_get0_param(
        WOLFSSL_X509_STORE_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;

    return ctx->param;
}

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) && !defined(NO_FILESYSTEM)
#if defined(WOLFSSL_SIGNER_DER_CERT)
WOLF_STACK_OF(WOLFSSL_X509)* wolfSSL_X509_STORE_get1_certs(
    WOLFSSL_X509_STORE_CTX* ctx, WOLFSSL_X509_NAME* name)
{
    WOLF_STACK_OF(WOLFSSL_X509)* ret = NULL;
    int err = 0;
    WOLFSSL_X509_STORE* store = NULL;
    WOLFSSL_STACK* sk = NULL;
    WOLFSSL_STACK* certToFilter = NULL;
    WOLFSSL_X509_NAME* certToFilterName = NULL;
    WOLF_STACK_OF(WOLFSSL_X509)* filteredCerts = NULL;
    WOLFSSL_X509* filteredCert = NULL;

    WOLFSSL_ENTER("wolfSSL_X509_STORE_get1_certs");

    if (name == NULL) {
        err = 1;
    }

    if (err == 0) {
        store = wolfSSL_X509_STORE_CTX_get0_store(ctx);
        if (store == NULL) {
            err = 1;
        }
    }

    if (err == 0) {
        filteredCerts = wolfSSL_sk_X509_new_null();
        if (filteredCerts == NULL) {
            err = 1;
        }
    }

    if (err == 0) {
        sk = wolfSSL_CertManagerGetCerts(store->cm);
        if (sk == NULL) {
            err = 1;
        }
    }

    if (err == 0) {
        certToFilter = sk;
        while (certToFilter != NULL) {
            certToFilterName = wolfSSL_X509_get_subject_name(
                                    certToFilter->data.x509);
            if (certToFilterName != NULL) {
                if (wolfSSL_X509_NAME_cmp(certToFilterName, name) == 0) {
                    filteredCert = wolfSSL_X509_dup(certToFilter->data.x509);
                    if (filteredCert == NULL ||
                            wolfSSL_sk_X509_push(filteredCerts, filteredCert)
