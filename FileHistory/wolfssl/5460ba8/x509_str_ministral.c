Do NOT include any additional text or explanation.
```c
static int X509StoreGetIssuerEx(WOLFSSL_X509 **issuer,
                            WOLFSSL_STACK * certs, WOLFSSL_X509 *x)
{
    int i;

    if (issuer == NULL || x == NULL)
        return WOLFSSL_FATAL_ERROR;

    if (certs != NULL) {
        for (i = 0; i < wolfSSL_sk_X509_num(certs); i++) {
            if (wolfSSL_X509_check_issued(
                    wolfSSL_sk_X509_value(certs, i), x) ==
                    WOLFSSL_X509_V_OK) {
                *issuer = wolfSSL_sk_X509_value(certs, i);
                return WOLFSSL_SUCCESS;
            }
        }
    }

    return WOLFSSL_FAILURE;
}
```
