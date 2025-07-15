static long ssl_sni_callback(SSL* p_ssl, int* p_al, void* p_arg)
{
    const char* servername = SSL_get_servername(p_ssl, TLSEXT_NAMETYPE_host_name);
    if (!servername) {
        *p_al = SSL_AD_UNRECOGNIZED_NAME;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    struct vsf_session* p_sess = (struct vsf_session*)p_arg;
    if (strcmp(servername, tunable_ssl_sni_hostname) != 0) {
        // Log the mismatch if needed
        if (tunable_debug_ssl) {
            str_alloc_text(&debug_str, "SNI mismatch: expected ");
            str_append_text(&debug_str, tunable_ssl_sni_hostname);
            str_append_text(&debug_str, ", but got ");
            str_append_text(&debug_str, servername);
            vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
        }
        *p_al = SSL_AD_UNRECOGNIZED_NAME;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    // If match is successful, proceed normally
    SSL_set_tlsext_host_name(p_ssl, servername);
    return SSL_TLSEXT_ERR_OK;
}