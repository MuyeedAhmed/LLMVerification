static long
ssl_sni_callback(SSL* p_ssl, int* p_al, void* p_arg)
{
  static struct mystr s_sni_expected_hostname;
  static struct mystr s_sni_received_hostname;

  int servername_type;
  const char* p_sni_servername;
  struct vsf_session* p_sess = (struct vsf_session*) p_arg;
  int is_ok = 0;

  (void) p_ssl;
  (void) p_arg;

  if (tunable_ssl_sni_hostname)
  {
    str_alloc_text(&s_sni_expected_hostname, tunable_ssl_sni_hostname);
  }

  /* The OpenSSL documentation says it is pre-initialized like this, but set
   * it just in case.
   */
  *p_al = SSL_AD_UNRECOGNIZED_NAME;

  servername_type = SSL_get_servername_type(p_ssl);
  p_sni_servername = SSL_get_servername(p_ssl, TLSEXT_NAMETYPE_host_name);
  if (p_sni_servername != NULL) {
    str_alloc_text(&s_sni_received_hostname, p_sni_servername);
  }

  if (str_isempty(&s_sni_expected_hostname))
  {
    is_ok = 1;
  }
  else if (servername_type != TLSEXT_NAMETYPE_host_name)
  {
    /* Fail. */
    str_alloc_text(&debug_str, "SNI bad type: ");
    str_append_ulong(&debug_str, servername_type);
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
  else
  {
    if (!str_strcmp(&s_sni_expected_hostname, &s_sni_received_hostname))
    {
      is_ok = 1;
    }
    else
    {
      str_alloc_text(&debug_str, "SNI rejection");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    }
  }

  if (!is_ok || tunable_debug_ssl)
  {
    str_alloc_text(&debug_str, "SNI hostname: ");
    str_append_str(&debug_str, &s_sni_received_hostname);
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }

  if (is_ok)
  {
    return SSL_TLSEXT_ERR_OK;
  }
  else
  {
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }
}