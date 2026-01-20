Do not repeat the problem statement. Do not include any other text. Do not include any explanations. Do not include any markdown formatting.

----- BEGIN modified.c -----
/* wc_pkcs11.c
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

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#ifdef HAVE_PKCS11

#ifndef HAVE_PKCS11_STATIC
#include <dlfcn.h>
#endif

#include <wolfssl/wolfcrypt/wc_pkcs11.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/logging.h>
#ifndef NO_RSA
    #include <wolfssl/wolfcrypt/rsa.h>
#endif
#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif

#ifndef WOLFSSL_HAVE_ECC_KEY_GET_PRIV
    /* FIPS build has replaced ecc.h. */
    #define wc_ecc_key_get_priv(key) (&((key)->k))
    #define WOLFSSL_HAVE_ECC_KEY_GET_PRIV
#endif

#if defined(NO_PKCS11_RSA) && !defined(NO_RSA)
    #define NO_RSA
#endif
#if defined(NO_PKCS11_ECC) && defined(HAVE_ECC)
    #undef HAVE_ECC
#endif
#if defined(NO_PKCS11_AES) && !defined(NO_AES)
    #define NO_AES
#endif
#if defined(NO_PKCS11_AESGCM) && defined(HAVE_AESGCM)
    #undef HAVE_AESGCM
#endif
#if defined(NO_PKCS11_AESCBC) && defined(HAVE_AES_CBC)
    #undef HAVE_AES_CBC
#endif
#if defined(NO_PKCS11_HMAC) && !defined(NO_HMAC)
    #define NO_HMAC
#endif
#if defined(NO_PKCS11_RNG) && !defined(WC_NO_RNG)
    #define WC_NO_RNG
#endif


/* Maximum length of the EC parameter string. */
#define MAX_EC_PARAM_LEN   16


#if defined(HAVE_ECC) && !defined(NO_PKCS11_ECDH)
/* Pointer to false required for templates. */
static CK_BBOOL ckFalse = CK_FALSE;
#endif
#if !defined(NO_RSA) || defined(HAVE_ECC) || (!defined(NO_AES) && \
           (defined(HAVE_AESGCM) || defined(HAVE_AES_CBC))) || !defined(NO_HMAC)
/* Pointer to true required for templates. */
static CK_BBOOL ckTrue  = CK_TRUE;
#endif

#ifndef NO_RSA
/* Pointer to RSA key type required for templates. */
static CK_KEY_TYPE rsaKeyType  = CKK_RSA;
#endif
#ifdef HAVE_ECC
/* Pointer to EC key type required for templates. */
static CK_KEY_TYPE ecKeyType   = CKK_EC;
#endif
#if !defined(NO_RSA) || defined(HAVE_ECC)
/* Pointer to public key class required for templates. */
static CK_OBJECT_CLASS pubKeyClass     = CKO_PUBLIC_KEY;
/* Pointer to private key class required for templates. */
static CK_OBJECT_CLASS privKeyClass    = CKO_PRIVATE_KEY;
#endif
#if (!defined(NO_AES) && (defined(HAVE_AESGCM) || defined(HAVE_AES_CBC))) || \
            !defined(NO_HMAC) || (defined(HAVE_ECC) && !defined(NO_PKCS11_ECDH))
/* Pointer to secret key class required for templates. */
static CK_OBJECT_CLASS secretKeyClass  = CKO_SECRET_KEY;
#endif

#ifdef WOLFSSL_DEBUG_PKCS11
/* Enable logging of PKCS#11 calls and return value. */
#define PKCS11_RV(op, rv)       pkcs11_rv(op, rv)
/* Enable logging of PKCS#11 calls and value. */
#define PKCS11_VAL(op, val)     pkcs11_val(op, val)
/* Enable logging of PKCS#11 template. */
#define PKCS11_DUMP_TEMPLATE(name, templ, cnt)  \
    pkcs11_dump_template(name, templ, cnt)

/* Formats of template items - used to instruct how to log information. */
enum PKCS11_TYPE_FORMATS {
    PKCS11_FMT_BOOLEAN,
    PKCS11_FMT_CLASS,
    PKCS11_FMT_KEY_TYPE,
    PKCS11_FMT_STRING,
    PKCS11_FMT_NUMBER,
    PKCS11_FMT_DATA,
    PKCS11_FMT_POINTER
};
/* Information for logging a template item. */
static struct PKCS11_TYPE_STR {
    /** Attribute type in template. */
    CK_ATTRIBUTE_TYPE type;
    /** String to log corresponding to attribute type. */
    const char* str;
    /** Format of data associated with template item. */
    int format;
} typeStr[] = {
    { CKA_CLASS,            "CKA_CLASS",              PKCS11_FMT_CLASS      },
    { CKA_TOKEN,            "CKA_TOKEN",              PKCS11_FMT_POINTER    },
    { CKA_PRIVATE,          "CKA_PRIVATE",            PKCS11_FMT_BOOLEAN    },
    { CKA_LABEL,            "CKA_LABEL",              PKCS11_FMT_STRING     },
    { CKA_VALUE,            "CKA_VALUE",              PKCS11_FMT_DATA       },
    { CKA_OBJECT_ID,        "CKA_OBJECT_ID",          PKCS11_FMT_POINTER    },
    { CKA_KEY_TYPE,         "CKA_KEY_TYPE",           PKCS11_FMT_KEY_TYPE   },
    { CKA_ID,               "CKA_ID",                 PKCS11_FMT_DATA       },
    { CKA_SENSITIVE,        "CKA_SENSITIVE",          PKCS11_FMT_BOOLEAN    },
    { CKA_ENCRYPT,          "CKA_ENCRYPT",            PKCS11_FMT_BOOLEAN    },
    { CKA_DECRYPT,          "CKA_DECRYPT",            PKCS11_FMT_BOOLEAN    },
    { CKA_SIGN,             "CKA_SIGN",               PKCS11_FMT_BOOLEAN    },
    { CKA_VERIFY,           "CKA_VERIFY",             PKCS11_FMT_BOOLEAN    },
    { CKA_DERIVE,           "CKA_DERIVE",             PKCS11_FMT_BOOLEAN    },
    { CKA_MODULUS_BITS,     "CKA_MODULUS_BITS",       PKCS11_FMT_NUMBER     },
    { CKA_MODULUS,          "CKA_MODULUS",            PKCS11_FMT_DATA       },
    { CKA_PUBLIC_EXPONENT,  "CKA_PUBLIC_EXPONENT",    PKCS11_FMT_DATA       },
    { CKA_PRIVATE_EXPONENT, "CKA_PRIVATE_EXPONENT",   PKCS11_FMT_DATA       },
    { CKA_PRIME_1,          "CKA_PRIME_1",            PKCS11_FMT_DATA       },
    { CKA_PRIME_2,          "CKA_PRIME_2",            PKCS11_FMT_DATA       },
    { CKA_EXPONENT_1,       "CKA_EXPONENT_1",         PKCS11_FMT_DATA       },
    { CKA_EXPONENT_2,       "CKA_EXPONENT_2",         PKCS11_FMT_DATA       },
    { CKA_VALUE_LEN,        "CKA_VALUE_LEN",          PKCS11_FMT_NUMBER     },
    { CKA_COEFFICIENT,      "CKA_COEFFICIENT",        PKCS11_FMT_DATA       },
    { CKA_EXTRACTABLE,      "CKA_EXTRACTABLE",        PKCS11_FMT_BOOLEAN    },
    { CKA_EC_PARAMS,        "CKA_EC_PARAMS",          PKCS11_FMT_DATA       },
    { CKA_EC_POINT,         "CKA_EC_POINT",           PKCS11_FMT_DATA       },
};
/* Count of known attribute types for logging. */
#define PKCS11_TYPE_STR_CNT  ((int)(sizeof(typeStr) / sizeof(*typeStr)))

/*
 * Dump/log the PKCS #11 template.
 *
 * This is only for debugging purposes. Only the values needed are recognised.
 *
 * @param  [in]  name   PKCS #11 template name.
 * @param  [in]  templ  PKCS #11 template to dump.
 * @param  [in]  cnt    Count of template entries.
 */
static void pkcs11_dump_template(const char* name, CK_ATTRIBUTE* templ,
                                 CK_ULONG cnt)
{
    CK_ULONG i;
    int j;
    char line[80];
    char type[25];
    int format;
    CK_KEY_TYPE keyType;
    CK_OBJECT_CLASS keyClass;

    WOLFSSL_MSG(name);

    for (i = 0; i < cnt; i++) {
        format = PKCS11_FMT_POINTER;

        for (j = 0; j < PKCS11_TYPE_STR_CNT; j++) {
            if (templ[i].type == typeStr[j].type) {
                XSNPRINTF(type, sizeof(type), "%s", typeStr[j].str);
                format = typeStr[j].format;
                break;
            }
        }
        if (j == PKCS11_TYPE_STR_CNT) {
            XSNPRINTF(type, sizeof(type), "%08lxUL", templ[i].type);
        }

        switch (format) {
        case PKCS11_FMT_BOOLEAN:
#if !defined(NO_RSA) || defined(HAVE_ECC) || (!defined(NO_AES) && \
           (defined(HAVE_AESGCM) || defined(HAVE_AES_CBC))) || !defined(NO_HMAC)
            if (templ[i].pValue == &ckTrue) {
                XSNPRINTF(line, sizeof(line), "%25s: TRUE", type);
                WOLFSSL_MSG(line);
            }
            else
#endif
#if defined(HAVE_ECC) && !defined(NO_PKCS11_ECDH)
            if (templ[i].pValue == &ckFalse) {
                XSNPRINTF(line, sizeof(line), "%25s: FALSE", type);
                WOLFSSL_MSG(line);
            }
            else
#endif
            {
                XSNPRINTF(line, sizeof(line), "%25s: INVALID (%p)", type,
                          templ[i].pValue);
                WOLFSSL_MSG(line);
            }
            break;
        case PKCS11_FMT_CLASS:
            keyClass = *(CK_OBJECT_CLASS*)templ[i].pValue;
            if (keyClass == CKO_PUBLIC_KEY) {
                XSNPRINTF(line, sizeof(line), "%25s: PUBLIC", type);
                WOLFSSL_MSG(line);
            }
            else if (keyClass == CKO_PRIVATE_KEY) {
                XSNPRINTF(line, sizeof(line), "%25s: PRIVATE", type);
                WOLFSSL_MSG(line);
            }
            else if (keyClass == CKO_SECRET_KEY) {
                XSNPRINTF(line, sizeof(line), "%25s: SECRET", type);
                WOLFSSL_MSG(line);
            }
            else
            {
                XSNPRINTF(line, sizeof(line), "%25s: UNKNOWN (%p)", type,
                          templ[i].pValue);
                WOLFSSL_MSG(line);
            }
            break;
        case PKCS11_FMT_KEY_TYPE:
            keyType = *(CK_KEY_TYPE*)templ[i].pValue;
            switch (keyType) {
            case CKK_RSA:
                XSNPRINTF(line, sizeof(line), "%25s: RSA", type);
                break;
            case CKK_DH:
                XSNPRINTF(line, sizeof(line), "%25s: DH", type);
                break;
            case CKK_EC:
                XSNPRINTF(line, sizeof(line), "%25s: EC", type);
                break;
            case CKK_GENERIC_SECRET:
                XSNPRINTF(line, sizeof(line), "%25s: GENERIC_SECRET", type);
                break;
            case CKK_AES:
                XSNPRINTF(line, sizeof(line), "%25s: AES", type);
                break;
            case CKK_MD5_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: MD5_HMAC", type);
                break;
            case CKK_SHA_1_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: SHA_1_HMAC", type);
                break;
            case CKK_SHA256_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: SHA256_HMAC", type);
                break;
            case CKK_SHA384_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: SHA384_HMAC", type);
                break;
            case CKK_SHA512_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: SHA512_HMAC", type);
                break;
            case CKK_SHA224_HMAC:
                XSNPRINTF(line, sizeof(line), "%25s: SHA224_HMAC", type);
                break;
            default:
                XSNPRINTF(line, sizeof(line), "%25s: UNKNOWN (%08lx)", type,
                          keyType);
                break;
            }
            WOLFSSL_MSG(line);
            break;
        case PKCS11_FMT_STRING:
            XSNPRINTF(line, sizeof(line), "%25s: %s", type,
                      (char*)templ[i].pValue);
            WOLFSSL_MSG(line);
            break;
        case PKCS11_FMT_NUMBER:
            if (templ[i].ulValueLen <= 1) {
                XSNPRINTF(line, sizeof(line), "%25s: 0x%02x (%d)", type,
                          *(byte*)templ[i].pValue, *(byte*)templ[i].pValue);
            }
            else if (templ[i].ulValueLen <= 2) {
                XSNPRINTF(line, sizeof(line), "%25s: 0x%04x (%d)", type,
                          *(word16*)templ[i].pValue, *(word16*)templ[i].pValue);
            }
            else if (templ[i].ulValueLen <= 4) {
                XSNPRINTF(line, sizeof(line), "%25s: 0x%08x (%d)", type,
                          *(word32*)templ[i].pValue, *(word32*)templ[i].pValue);
            }
            else if (templ[i].ulValueLen <= 8) {
                XSNPRINTF(line, sizeof(line), "%25s: 0x%016lx (%ld)", type,
                          *(word64*)templ[i].pValue, *(word64*)templ[i].pValue);
            }
            else {
                XSNPRINTF(line, sizeof(line), "%25s: INVALID (%ld)", type,
                          templ[i].ulValueLen);
            }
            WOLFSSL_MSG(line);
            break;
        case PKCS11_FMT_DATA:
            XSNPRINTF(line, sizeof(line), "%25s: %ld", type,
                      templ[i].ulValueLen);
            WOLFSSL_MSG(line);
            if (templ[i].pValue == NULL) {
                XSNPRINTF(line, sizeof(line), "%27s(nil)", "");
                WOLFSSL_MSG(line);
                break;
            }
            XSNPRINTF(line, sizeof(line), "%27s", "");
            for (j = 0; j < (int)templ[i].ulValueLen && j < 80; j++) {
                char hex[6];
                XSNPRINTF(hex, sizeof(hex), "0x%02x,",
                          ((byte*)templ[i].pValue)[j]);
                XSTRNCAT(line, hex, 5);
                if ((j % 8) == 7) {
                    WOLFSSL_MSG(line);
                    XSNPRINTF(line, sizeof(line), "%27s", "");
                }
            }
            if (j == (int)templ[i].ulValueLen) {
                if ((j % 8) != 0) {
                    WOLFSSL_MSG(line);
                }
            }
            else if (j < (int)templ[i].ulValueLen) {
                XSNPRINTF(line, sizeof(line), "%27s...", "");
                WOLFSSL_MSG(line);
            }
            break;
        case PKCS11_FMT_POINTER:
            XSNPRINTF(line, sizeof(line), "%25s: %p %ld", type, templ[i].pValue,
                      templ[i].ulValueLen);
            WOLFSSL_MSG(line);
            break;
        }
    }
}

/*
 * Log a PKCS #11 return value with the name of function called.
 *
 * This is only for debugging purposes. Only the values needed are recognized.
 *
 * @param  [in]  op  PKCS #11 operation that was attempted.
 * @param  [in]  rv  PKCS #11 return value.
 */
static void pkcs11_rv(const char* op, CK_RV rv)
{
    char line[80];

    if (rv == CKR_OK) {
        XSNPRINTF(line, 80, "%s: OK", op);
    }
    else if (rv == CKR_MECHANISM_INVALID) {
        XSNPRINTF(line, 80, "%s: MECHANISM_INVALID", op);
    }
    else if (rv == CKR_SIGNATURE_INVALID) {
        XSNPRINTF(line, 80, "%s: SIGNATURE_INVALID", op);
    }
    else {
        XSNPRINTF(line, 80, "%s: %08lxUL (FAILED)", op, rv);
    }

    WOLFSSL_MSG(line);
}

/*
 * Log a value from a PKCS #11 operation.
 *
 * This is only for debugging purposes.
 *
 * @param  [in]  op   PKCS #11 operation that was attempted.
 * @param  [in]  val  Value to log.
 */
static void pkcs11_val(const char* op, CK_ULONG val)
{
    char line[80];

    XSNPRINTF(line, 80, "%s: %ld", op, val);

    WOLFSSL_MSG(line);
}
#else
/* Disable logging of PKCS#11 calls and return value. */
#define PKCS11_RV(op, ev) WC_DO_NOTHING
/* Disable logging of PKCS#11 calls and value. */
#define PKCS11_VAL(op, val) WC_DO_NOTHING
/* Disable logging of PKCS#11 template. */
#define PKCS11_DUMP_TEMPLATE(name, templ, cnt) WC_DO_NOTHING
#endif

/**
 * Load library, get function list and initialize PKCS#11.
 *
 * @param  [in]  dev      Device object.
 * @param  [in]  library  Library name including path.
 * @param  [in]  heap     Heap hint.
 * @return  BAD_FUNC_ARG when dev or library are NULL pointers.
 * @return  BAD_PATH_ERROR when dynamic library cannot be opened.
 * @return  WC_INIT_E when the initialization PKCS#11 fails.
 * @return  WC_HW_E when unable to get PKCS#11 function list.
 * @return  0 on success.
 */
int wc_Pkcs11_Initialize(Pkcs11Dev* dev, const char* library, void* heap)
{
    return wc_Pkcs11_Initialize_ex(dev, library, heap, NULL);
}

/**
 * Load library, get function list and initialize PKCS#11.
 *
 * @param  [in]   dev      Device object.
 * @param  [in]   library  Library name including path.
 * @param  [in]   heap     Heap hint.
 * @param  [out]  rvp      PKCS#11 return value. Last return value seen.
 *                         May be NULL.
 * @return  BAD_FUNC_ARG when dev or library are NULL pointers.
 * @return  BAD_PATH_ERROR when dynamic library cannot be opened.
 * @return  WC_INIT_E when the initialization PKCS#11 fails.
 * @return  WC_HW_E when unable to get PKCS#11 function list.
 * @return  0 on success.
 */
int wc_Pkcs11_Initialize_ex(Pkcs11Dev* dev, const char* library, void* heap,
                            CK_RV* rvp)
{
    int                  ret = 0;
    CK_RV                rv = CKR_OK;
#ifndef HAVE_PKCS11_STATIC
    void*                func;
#endif
    CK_C_INITIALIZE_ARGS args;

    if (dev == NULL || library == NULL)
        ret = BAD_FUNC_ARG;

    if (ret == 0) {
        dev->heap = heap;
#ifndef HAVE_PKCS11_STATIC
        dev->dlHandle = dlopen(library, RTLD_NOW | RTLD_LOCAL);
        if (dev->dlHandle == NULL) {
            WOLFSSL_MSG(dlerror());
            ret = BAD_PATH_ERROR;
        }
    }

    if (ret == 0) {
        dev->func = NULL;
        func = dlsym(dev->dlHandle, "C_GetFunctionList");
        if (func == NULL) {
            WOLFSSL_MSG(dlerror());
            ret = WC_HW_E;
        }
    }
    if (ret == 0) {
        rv = ((CK_C_GetFunctionList)func)(&dev->func);
#else
        rv = C_GetFunctionList(&dev->func);
#endif
        if (rv != CKR_OK) {
            PKCS11_RV("CK_C_GetFunctionList", ret);
            ret = WC_HW_E;
        }
    }

    if (ret == 0) {
        XMEMSET(&args, 0x00, sizeof(args));
        args.flags = CKF_OS_LOCKING_OK;
        rv = dev->func->C_Initialize(&args);
        if (rv != CKR_OK) {
            PKCS11_RV("C_Initialize", ret);
            ret = WC_INIT_E;
        }
    }

    if (rvp != NULL) {
        *rvp = rv;
    }

    if (ret != 0) {
        wc_Pkcs11_Finalize(dev);
    }

    return ret;
}

/**
 * Close the Pkcs#11 library.
 *
 * @param  [in]  dev  Device object.
 */
void wc_Pkcs11_Finalize(Pkcs11Dev* dev)
{
    if (dev != NULL
#ifndef HAVE_PKCS11_STATIC
        && dev->dlHandle != NULL
#endif
        ) {
        if (dev->func != NULL) {
            dev->func->C_Finalize(NULL);
            dev->func = NULL;
        }
#ifndef HAVE_PKCS11_STATIC
        dlclose(dev->dlHandle);
        dev->dlHandle = NULL;
#endif
    }
}

/* lookup by token name and return slotId or (-1) if not found */
static int Pkcs11Slot_FindByTokenName(Pkcs11Dev* dev,
    const char* tokenName, size_t tokenNameSz)
{
    CK_RV         rv;
    CK_ULONG      slotCnt = 0;
    CK_TOKEN_INFO tinfo;
    int           slotId = -1;
    rv = dev->func->C_GetSlotList(CK_TRUE, NULL, &slotCnt);
    if (rv == CKR_OK) {
        for (slotId = 0; slotId < (int)slotCnt; slotId++) {
            rv = dev->func->C_GetTokenInfo(slotId, &tinfo);
            PKCS11_RV("C_GetTokenInfo", rv);
            if (rv == CKR_OK &&
                XMEMCMP(tinfo.label, tokenName, tokenNameSz) == 0) {
                return slotId;
            }
        }
    }
    return -1;
}

/* lookup by slotId or tokenName */
static int Pkcs11Token_Init(Pkcs11Token* token, Pkcs11Dev* dev, int slotId,
    const char* tokenName, size_t tokenNameSz)
{
    int         ret = 0;
    CK_RV       rv;
    CK_SLOT_ID* slot = NULL;
    CK_ULONG    slotCnt = 0;

    if (token == NULL || dev == NULL) {
        ret = BAD_FUNC_ARG;
    }

    if (ret == 0) {
        if (slotId < 0) {
            rv = dev->func->C_GetSlotList(CK_TRUE, NULL, &slotCnt);
            PKCS11_RV("C_GetSlotList", rv);
            if (rv != CKR_OK) {
                ret = WC_HW_E;
            }
            if (ret == 0) {
                slot = (CK_SLOT_ID*)XMALLOC(slotCnt * sizeof(*slot), dev->heap,
                                                       DYNAMIC_TYPE_TMP_BUFFER);
                if (slot == NULL)
                    ret = MEMORY_E;
            }
            if (ret == 0) {
                rv = dev->func->C_GetSlotList(CK_TRUE, slot, &slotCnt);
                PKCS11_RV("C_GetSlotList", rv);
                if (rv != CKR_OK) {
                    ret = WC_HW_E;
                }
            }
            if (ret == 0) {
                if (tokenName != NULL && tokenNameSz > 0) {
                    /* find based on token name */
                    slotId = Pkcs11Slot_FindByTokenName(dev,
                        tokenName, tokenNameSz);
                }
                else {
                    /* Use first available slot with a token. */
                    slotId = (int)slot[0];
                }
            }
        }
        else {
            /* verify slotId is valid */
            CK_SLOT_INFO sinfo;
            rv = dev->func->C_GetSlotInfo(slotId, &sinfo);
            PKCS11_RV("C_GetSlotInfo", rv);
            if (rv != CKR_OK) {
                ret = WC_INIT_E;
            }
        }
    }
    if (ret == 0) {
        token->func = dev->func;
        token->slotId = (CK_SLOT_ID)slotId;
        token->handle = NULL_PTR;
        token->userPin = NULL_PTR;
        token->userPinSz = 0;
        token->userPinLogin = 0;
    }

    if (slot != NULL) {
        XFREE(slot, dev->heap, DYNAMIC_TYPE_TMP_BUFFER);
    }

    return ret;
}

/**
 * Set up a token for use. Lookup by slotId or tokenName. Set User PIN.
 *
 * @param  [in]  token      Token object.
 * @param  [in]  dev        PKCS#11 device object.
 * @param  [in]  slotId     Slot number of the token.<br>
 *                          Passing -1 uses the first available slot.
 * @param  [in]  tokenName  Name of token to initialize (optional)
 * @param  [in]  userPin    PIN to use to login as user.
 * @param  [in]  userPinSz  Number of bytes in PIN.
 * @return  BAD_FUNC_ARG when token, dev and/or tokenName is NULL.
 * @return  WC_INIT_E when initializing token fails.
 * @return  WC_HW_E when another PKCS#11 library call fails.
 * @return  0 on success.
 */
int wc_Pkcs11Token_Init(Pkcs11Token* token, Pkcs11Dev* dev, int slotId,
    const char* tokenName, const unsigned char* userPin, int userPinSz)
{
    int ret;
    size_t tokenNameSz = 0;

    if (tokenName != NULL) {
        tokenNameSz = XSTRLEN(tokenName);
    }
    ret = Pkcs11Token_Init(token, dev, slotId, tokenName, tokenNameSz);
    if (ret == 0) {
        token->userPin = (CK_UTF8CHAR_PTR)userPin;
        token->userPinSz = (CK_ULONG)userPinSz;
        token->userPinLogin = 1;
    }

    return ret;
}

/**
 * Set up a token for use. Lookup by slotId or tokenName.
 *
 * @param  [in]  token      Token object.
 * @param  [in]  dev        PKCS#11 device object.
 * @param  [in]  slotId     Slot number of the token.<br>
 *                          Passing -1 uses the first available slot.
 * @param  [in]  tokenName  Name of token to initialize (optional)
 * @return  BAD_FUNC_ARG when token, dev and/or tokenName is NULL.
 * @return  WC_INIT_E when initializing token fails.
 * @return  WC_HW_E when another PKCS#11 library call fails.
 * @return  0 on success.
 */
int wc_Pkcs11Token_Init_NoLogin(Pkcs11Token* token, Pkcs11Dev* dev, int slotId,
    const char* tokenName)
{
    size_t tokenNameSz = 0;
    if (tokenName != NULL) {
        tokenNameSz = XSTRLEN(tokenName);
    }
    return Pkcs11Token_Init(token, dev, slotId, tokenName, tokenNameSz);
}

/**
 * Set up a token for use. Lookup by slotId or tokenName/size. Set User PIN.
 *
 * @param  [in]  token       Token object.
 * @param  [in]  dev         PKCS#11 device object.
 * @param  [in]  tokenName   Name of token to initialize.
 * @param  [in]  tokenNameSz Name size for token
 * @param  [in]  userPin     PIN to use to login as user.
 * @param  [in]  userPinSz   Number of bytes in PIN.
 * @return  BAD_FUNC_ARG when token, dev and/or tokenName is NULL.
 * @return  WC_INIT_E when initializing token fails.
 * @return  WC_HW_E when another PKCS#11 library call fails.
 * @return  0 on success.
 */
int wc_Pkcs11Token_InitName(Pkcs11Token* token, Pkcs11Dev* dev,
    const char* tokenName, int tokenNameSz,
    const unsigned char* userPin, int userPinSz)
{
    int ret = Pkcs11Token_Init(token, dev, -1, tokenName, (size_t)tokenNameSz);
    if (ret == 0) {
        token->userPin = (CK_UTF8CHAR_PTR)userPin;
        token->userPinSz = (CK_ULONG)userPinSz;
        token->userPinLogin = 1;
    }

    return ret;
}

/**
 * Set up a token for use. Lookup by slotId or tokenName/size.
 *
 * @param  [in]  token       Token object.
 * @param  [in]  dev         PKCS#11 device object.
 * @param  [in]  tokenName   Name of token to initialize.
 * @param  [in]  tokenNameSz Name size for token
 * @param  [in]  userPin     PIN to use to login as user.
 * @param  [in]  userPinSz   Number of bytes in PIN.
 * @return  BAD_FUNC_ARG when token, dev and/or tokenName is NULL.
 * @return  WC_INIT_E when initializing token fails.
 * @return  WC_HW_E when another PKCS#11 library call fails.
 * @return  0 on success.
 */
int wc_Pkcs11Token_InitName_NoLogin(Pkcs11Token* token, Pkcs11Dev* dev,
    const char* tokenName, int tokenNameSz)
{
    return Pkcs11Token_Init(token, dev, -1, tokenName, (size_t)tokenNameSz);
}

/**
 * Finalize token.
 * Closes all sessions on token.
 *
 * @param  [in]  token  Token object.
 */
void wc_Pkcs11Token_Final(Pkcs11Token* token)
{
    if (token != NULL && token->func != NULL) {
        token->func->C_CloseAllSessions(token->slotId);
        token->handle = NULL_PTR;
        ForceZero(token->userPin, (word32)token->userPinSz);
    }
}

/**
 * Open a session on a token.
 *
 * @param  [in]  token      Token object.
 * @param  [in]  session    Session object.
 * @param  [in]  readWrite  Boolean indicating to open session for Read/Write.
 * @return  BAD_FUNC_ARG when token or session is NULL.
 * @return  WC_HW_E when opening the session fails.
 * @return  0 on success.
 */
static int Pkcs11OpenSession(Pkcs11Token* token, Pkcs11Session* session,
                             int readWrite)
{
    int   ret = 0;
    CK_RV rv;

    if (token == NULL || session == NULL)
        ret = BAD_FUNC_ARG;

    if (ret == 0) {
        if (token->handle != NULL_PTR)
            session->handle = token->handle;
        else {
            /* Create a new session. */
            CK_FLAGS flags = CKF_SERIAL_SESSION;

            if (readWrite)
                flags |= CKF_RW_SESSION;

            rv = token->func->C_OpenSession(token->slotId, flags,
                                            (CK_VOID_PTR)NULL, (CK_NOTIFY)NULL,
                                            &session->handle);
            PKCS11_RV("C_OpenSession", rv);
            if (rv != CKR_OK) {
                ret = WC_HW_E;
            }
            if (ret == 0 && token->userPinLogin) {
                rv = token->func->C_Login(session->handle, CKU_USER,
                                              token->userPin, token->userPinSz);
                PKCS11_RV("C_Login", rv);
                if (rv != CKR_OK) {
                    ret = WC_HW_E;
                }
            }
        }
    }
    if (ret == 0) {
        session->func = token->func;
        session->slotId = token->slotId;
    }

    return ret;
}

/**
 * Close a session on a token.
 * Won't close a session created externally.
 *
 * @param  [in]  token    Token object.
 * @param  [in]  session  Session object.
 */
static void Pkcs11CloseSession(Pkcs11Token* token, Pkcs11Session* session)
{
    if (token != NULL && session != NULL && token->handle != session->handle) {
        if (token->userPin != NULL)
            session->func->C_Logout(session->handle);
        session->func->C_CloseSession(session->handle);
    }
}

/**
 * Open a session on the token to be used for all operations.
 *
 * @param  [in]  token      Token object.
 * @param  [in]  readWrite  Boolean indicating to open session for Read/Write.
 * @return  BAD_FUNC_ARG when token is NULL.
 * @return  WC_HW_E when opening the session fails.
 * @return  0 on success.
 */
int wc_Pkcs11Token_Open(Pkcs11Token* token, int readWrite)
{
    int ret = 0;
    Pkcs11Session session;

    if (token == NULL)
        ret = BAD_FUNC_ARG;

    if (ret == 0) {
        ret = Pkcs11OpenSession(token, &session, readWrite);
        token->handle = session.handle;
    }

    return ret;
}

/**
 * Close the token's session.
 * All object, like keys, will be destroyed.
 *
 * @param  [in]  token  Token object.
 */
void wc_Pkcs11Token_Close(Pkcs11Token* token)
{
    Pkcs11Session session;

    if (token != NULL) {
        session.func = token->func;
        session.handle = token->handle;
        token->handle = NULL_PTR;
        Pkcs11CloseSession(token, &session);
    }
}


#if (!defined(NO_AES) && (defined(HAVE_AESGCM) || defined(HAVE_AES_CBC))) || \
                                                               !defined(NO_HMAC)
/*
 * Create a secret key.
 *
 * @param  [out]  key       Handle to key object.
 * @param  [in]   session   Session object.
 * @param  [in]   keyType   Type of secret key to create.
 * @param  [in]   data      Data of the secret key.
 * @param  [in]   len       Length of data in bytes.
 * @param  [in]   id        Identifier to set against key.
 * @param  [in]   idLen     Length of identifier.
 * @param  [in]   label     Label to set against key.
 * @param  [in]   labelLen  Length of label.
 * @param  [in]   op        Operation to support with
