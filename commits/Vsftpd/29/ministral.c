static long ssl_sni_callback(SSL* p_ssl, int* p_al, void* p_arg)
{
    const char* sni_host = SSL_get_servername(p_ssl, TLSEXT_NAMETYPE_host_name);
    if (sni_host == NULL)
    {
        return SSL_TLSEXT_ERR_NOACK;
    }

    struct vsf_session* p_sess = (struct vsf_session*)p_arg;
    if (p_sess->ssl_sni_hostname != NULL &&
        str_strcmp(&p_sess->ssl_sni_hostname, sni_host) != 0)
    {
        *p_al = SSL_AD_HANDSHAKE_FAILURE;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    return SSL_TLSEXT_ERR_OK;
}
