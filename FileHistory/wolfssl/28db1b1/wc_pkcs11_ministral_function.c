Do not include any additional text or explanations.
```c
#ifndef NO_RSA
/**
 * Create a PKCS#11 object containing the RSA private key data.
 *
 * @param  [out]  privateKey  Handle to private key object.
 * @param  [in]   session     Session object.
 * @param  [in]   rsaKey      RSA key with private key data.
 * @return  WC_HW_E when a PKCS#11 library call fails.
 * @return  0 on success.
 */
static int Pkcs11CreateRsaPrivateKey(CK_OBJECT_HANDLE* privateKey,
                                     Pkcs11Session* session,
                                     RsaKey* rsaKey, int permanent)
{
    int             ret = 0;
    CK_RV           rv;
    /* Empty entries for optional label/ID. */
    CK_ATTRIBUTE    keyTemplate[] = {
        { CKA_CLASS,            &privKeyClass, sizeof(privKeyClass) },
        { CKA_KEY_TYPE,         &rsaKeyType,   sizeof(rsaKeyType)   },
        { CKA_DECRYPT,          &ckTrue,       sizeof(ckTrue)       },
        { CKA_SIGN,             &ckTrue,       sizeof(ckTrue)       },
        { CKA_MODULUS,          NULL,          0                    },
        { CKA_PRIVATE_EXPONENT, NULL,          0                    },
        { CKA_PRIME_1,          NULL,          0                    },
        { CKA_PRIME_2,          NULL,          0                    },
        { CKA_EXPONENT_1,       NULL,          0                    },
        { CKA_EXPONENT_2,       NULL,          0                    },
        { CKA_COEFFICIENT,      NULL,          0                    },
        { CKA_PUBLIC_EXPONENT,  NULL,          0                    },
        { 0,                    NULL,          0                    },
        { 0,                    NULL,          0                    }
    };
    /* Mandatory entries + 2 optional. */
    CK_ULONG        keyTmplCnt = sizeof(keyTemplate) / sizeof(*keyTemplate) - 2;

    /* Set the modulus and private key data. */
    keyTemplate[ 4].pValue     = rsaKey->n.raw.buf;
    keyTemplate[ 4].ulValueLen = rsaKey->n.raw.len;
    keyTemplate[ 5].pValue     = rsaKey->d.raw.buf;
    keyTemplate[ 5].ulValueLen = rsaKey->d.raw.len;
    keyTemplate[ 6].pValue     = rsaKey->p.raw.buf;
    keyTemplate[ 6].ulValueLen = rsaKey->p.raw.len;
    keyTemplate[ 7].pValue     = rsaKey->q.raw.buf;
    keyTemplate[ 7].ulValueLen = rsaKey->q.raw.len;
    keyTemplate[ 8].pValue     = rsaKey->dP.raw.buf;
    keyTemplate[ 8].ulValueLen = rsaKey->dP.raw.len;
    keyTemplate[ 9].pValue     = rsaKey->dQ.raw.buf;
    keyTemplate[ 9].ulValueLen = rsaKey->dQ.raw.len;
    keyTemplate[10].pValue     = rsaKey->u.raw.buf;
    keyTemplate[10].ulValueLen = rsaKey->u.raw.len;
    keyTemplate[11].pValue     = rsaKey->e.raw.buf;
    keyTemplate[11].ulValueLen = rsaKey->e.raw.len;

    if (permanent && rsaKey->labelLen > 0) {
        keyTemplate[keyTmplCnt].type       = CKA_LABEL;
        keyTemplate[keyTmplCnt].pValue     = rsaKey->label;
        keyTemplate[keyTmplCnt].ulValueLen = rsaKey->labelLen;
        keyTmplCnt++;
    }
    if (permanent && rsaKey->idLen > 0) {
        keyTemplate[keyTmplCnt].type       = CKA_ID;
        keyTemplate[keyTmplCnt].pValue     = rsaKey->id;
        keyTemplate[keyTmplCnt].ulValueLen = rsaKey->idLen;
        keyTmplCnt++;
    }

    PKCS11_DUMP_TEMPLATE("RSA Private Key", keyTemplate, keyTmplCnt);
    rv = session->func->C_CreateObject(session->handle, keyTemplate, keyTmplCnt,
                                                                    privateKey);
    PKCS11_RV("C_CreateObject", rv);
    if (rv != CKR_OK) {
        ret = WC_HW_E;
    }

    return ret;
}
#endif /* !NO_RSA */
```
