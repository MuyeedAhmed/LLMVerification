No explanation needed.

```c
#ifdef WOLF_CRYPTO_CB
    static void printOutput(const char* strName, unsigned char* data,
            unsigned int dataSz)
    {
        #ifdef DEBUG_WOLFSSL
        WOLFSSL_MSG_EX("%s (%d):", strName,dataSz);
        WOLFSSL_BUFFER(data,dataSz);
        #else
        #if 0
            unsigned int i;
            int line = 1;

            printf("%s:\n",strName);
            printf("    ");
            for(i=1; i<=dataSz; i++)
            {
                printf(",0x%02X", data[i-1]);
                if(!(i%16) && i!= dataSz) printf("\n    ");
                else if(!(i%4)) printf(" ");
            }
            printf("\n");
        #else
            (void)strName;
            (void)data;
            (void)dataSz;
        #endif
        #endif
    }

    int wc_AriaCryptoCb(int devIdArg, wc_CryptoInfo* info, void* ctx)
    {
        int ret = WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE); /* return this to bypass HW and use SW */
        (void)ctx;

        if (info == NULL)
            return BAD_FUNC_ARG;

    #ifdef DEBUG_CRYPTOCB
        wc_CryptoCb_InfoString(info);
    #endif

        if (info->algo_type == WC_ALGO_TYPE_PK) {
            if (info->pk.type == WC_PK_TYPE_ECDSA_SIGN) {
                /* set devId to invalid, so software is used */
                info->pk.eccsign.key->devId = INVALID_DEVID;

                printOutput((char *)"eccsign.in (before)",
                            (byte *)info->pk.eccsign.in,info->pk.eccsign.inlen);
                printOutput((char *)"eccsign.out(before)",
                            (byte *)info->pk.eccsign.out,*(info->pk.eccsign.outlen));
                printOutput((char *)"eccsign.key(before)",
                            (byte *)info->pk.eccsign.key,sizeof(info->pk.eccsign.key));

                byte buf[ARIA_KEYASN1_MAXSZ];
                word32 bufSz = sizeof(buf);
                ret = wc_AriaSign((byte *)info->pk.eccsign.in,info->pk.eccsign.inlen,
                            buf,&bufSz,
                            info->pk.eccsign.key);
                if (ret != 0) {
                    ret = CRYPTOCB_UNAVAILABLE;
                } else {
                    memcpy(info->pk.eccsign.out, buf, bufSz);
                    *(info->pk.eccsign.outlen) = bufSz;
                }

                printOutput((char *)"eccsign.in (after)",
                            (byte *)info->pk.eccsign.in,info->pk.eccsign.inlen);
                printOutput((char *)"eccsign.out(after)",
                            (byte *)info->pk.eccsign.out,*(info->pk.eccsign.outlen));
                printOutput((char *)"eccsign.key(after)",
                            (byte *)info->pk.eccsign.key,sizeof(info->pk.eccsign.key));

                /* reset devId */
                info->pk.eccsign.key->devId = devIdArg;
            }
            else if (info->pk.type == WC_PK_TYPE_ECDSA_VERIFY) {
                /* set devId to invalid, so software is used */
                info->pk.eccverify.key->devId = INVALID_DEVID;

                printOutput((char *)"eccverify.sig (before)",
                            (byte *)info->pk.eccverify.sig,info->pk.eccverify.siglen);
                printOutput((char *)"eccverify.hash(before)",
                            (byte *)info->pk.eccverify.hash,info->pk.eccverify.hashlen);
                printOutput((char *)"eccverify.key (before)",
                            (byte *)info->pk.eccverify.key,sizeof(info->pk.eccverify.key));

                ret = wc_AriaVerify((byte *)info->pk.eccverify.sig,info->pk.eccverify.siglen,
                                (byte *)info->pk.eccverify.hash, info->pk.eccverify.hashlen,
                                info->pk.eccverify.res, info->pk.eccverify.key);

                printOutput((char *)"eccverify.sig (after)",
                            (byte *)info->pk.eccverify.sig,info->pk.eccverify.siglen);
                printOutput((char *)"eccverify.hash(after)",
                            (byte *)info->pk.eccverify.hash,info->pk.eccverify.hashlen);
                printOutput((char *)"eccverify.key (after)",
                            (byte *)info->pk.eccverify.key,sizeof(info->pk.eccverify.key));

                if (ret != 0)
                    ret = CRYPTOCB_UNAVAILABLE;
                /* reset devId */
                info->pk.eccverify.key->devId = devIdArg;
            }
            else if (info->pk.type == WC_PK_TYPE_ECDH) {
                /* set devId to invalid, so software is used */
                info->pk.ecdh.private_key->devId = INVALID_DEVID;

                ret = wc_AriaDerive(
                    info->pk.ecdh.private_key, info->pk.ecdh.public_key,
                    info->pk.ecdh.out, info->pk.ecdh.outlen);

                if (ret != 0)
                    ret = CRYPTOCB_UNAVAILABLE;
                /* reset devId */
                info->pk.ecdh.private_key->devId = devIdArg;
            }
        }
        else if (info->algo_type == WC_ALGO_TYPE_HASH) {
            if (info->hash.type == WC_HASH_TYPE_SHA256) {
                if (info->hash.sha256 == NULL)
                    return CRYPTOCB_UNAVAILABLE;

                /* set devId to invalid, so software is used */
                info->hash.sha256->devId = INVALID_DEVID;

                if (info->hash.sha256->hSession == NULL) {
                    ret = wc_AriaInitSha(&(info->hash.sha256->hSession), MC_ALGID_SHA256);
                }

                if ((ret == 0) ||
                    (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                    ret = wc_AriaShaUpdate(info->hash.sha256->hSession,
                                        (byte *) info->hash.in, info->hash.inSz);
                }
                if ((ret == 0) ||
                    (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                    MC_UINT digestSz = 32;
                    ret = wc_AriaShaFinal(info->hash.sha256->hSession,
                                        info->hash.digest, &digestSz);
                    if ((ret == 0) ||
                        (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                        ret = wc_AriaFree(&(info->hash.sha256->hSession), NULL);
                    }
                }
                if (ret != 0)
                    ret = CRYPTOCB_UNAVAILABLE;
                /* reset devId */
                info->hash.sha256->devId = devIdArg;
            }
            else if (info->hash.type == WC_HASH_TYPE_SHA384) {
                if (info->hash.sha384 == NULL)
                    return CRYPTOCB_UNAVAILABLE;

                /* set devId to invalid, so software is used */
                info->hash.sha384->devId = INVALID_DEVID;

                if (info->hash.sha384->hSession == NULL) {
                    ret = wc_AriaInitSha(&(info->hash.sha384->hSession), MC_ALGID_SHA384);
                }

                if ((ret == 0) ||
                    (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                    ret = wc_AriaShaUpdate(info->hash.sha384->hSession,
                                        (byte *) info->hash.in, info->hash.inSz);
                }
                if ((ret == 0) ||
                    (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                    MC_UINT digestSz = 48;
                    ret = wc_AriaShaFinal(info->hash.sha384->hSession,
                                        info->hash.digest, &digestSz);
                    if ((ret == 0) ||
                        (ret == WC_NO_ERR_TRACE(CRYPTOCB_UNAVAILABLE))) {
                        ret = wc_AriaFree(&(info->hash.sha384->hSession), NULL);
                    }
                }
                if (ret != 0) ret = CRYPTOCB_UNAVAILABLE;
                /* reset devId */
                info->hash.sha384->devId = devIdArg;
            }
        }

        return ret;
    }
#endif /* WOLF_CRYPTO_CB */
```
