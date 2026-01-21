static int ssl_alpn_callback(SSL* p_ssl, const unsigned char** p_out,
                             unsigned char* outlen, const unsigned char* p_in,
                             unsigned int inlen, void* p_arg)
{
    (void) p_ssl;
    (void) p_arg;

    if (inlen == 0) {
        /* No ALPN protocols offered by client, reject */
        return SSL_TLSEXT_ERR_NOACK;
    }

    /* Check if the offered protocol is 'ftp' */
    if (inlen == 4 && memcmp(p_in, "ftp\0", 4) == 0) {
        *p_out = (unsigned char*)"ftp";
        *outlen = 3;
        return SSL_TLSEXT_ERR_OK;
    }

    /* Reject any other protocol */
    return SSL_TLSEXT_ERR_NOACK;
}
