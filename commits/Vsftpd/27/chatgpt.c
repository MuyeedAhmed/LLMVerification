static int
ssl_alpn_callback(SSL* p_ssl,
                  const unsigned char** p_out,
                  unsigned char* outlen,
                  const unsigned char* p_in,
                  unsigned int inlen,
                  void* p_arg) {
  unsigned int i;
  struct vsf_session* p_sess = (struct vsf_session*) p_arg;
  int is_ok = 0;

  (void) p_ssl;

  for (i = 0; i + 4 <= inlen; i++) {
    if (p_in[i] == 3 && p_in[i + 1] == 'f' && p_in[i + 2] == 't' && p_in[i + 3] == 'p') {
      *p_out = &p_in[i];
      *outlen = 4;
      is_ok = 1;
      break;
    }
  }

  if (!is_ok) {
    str_alloc_text(&debug_str, "ALPN rejection");
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
  if (!is_ok || tunable_debug_ssl) {
    str_alloc_text(&debug_str, "ALPN data: ");
    for (i = 0; i < inlen; ++i) {
      str_append_char(&debug_str, p_in[i]);
    }
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }

  if (is_ok) {
    return SSL_TLSEXT_ERR_OK;
  } else {
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }
}