static long ssl_sni_callback(SSL* p_ssl, int* p_al, void* p_arg)
{
  const char* p_sni_hostname = SSL_get_servername(p_ssl, TLSEXT_NAMETYPE_host_name);
  if (p_sni_hostname == NULL)
  {
    *p_al = SSL_AD_UNRECOGNIZED_NAME;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }
  if (tunable_ssl_sni_hostname != NULL &&
      strcmp(p_sni_hostname, tunable_ssl_sni_hostname) != 0)
  {
    *p_al = SSL_AD_UNRECOGNIZED_NAME;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }
  return SSL_TLSEXT_ERR_OK;
}
