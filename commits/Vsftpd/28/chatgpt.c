static int ssl_alpn_callback(SSL* p_ssl, const unsigned char** p_out, unsigned char* outlen, const unsigned char* p_in, unsigned int inlen, void* p_arg) {
    const unsigned char *pos = p_in;
    unsigned int remaining = inlen;

    while (remaining > 0) {
        unsigned int len = *pos;
        pos++;
        if (len == 3 && memcmp(pos, "ftp", 3) == 0) {
            *p_out = pos;
            *outlen = len;
            return SSL_TLSEXT_ERR_OK;
        }
        pos += len;
        remaining -= (len + 1);
    }

    return SSL_TLSEXT_ERR_ALERT_FATAL;
}