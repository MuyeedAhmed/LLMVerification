static int ssl_alpn_callback(SSL* p_ssl, const unsigned char** p_out, unsigned char* outlen, const unsigned char* p_in, unsigned int inlen, void* p_arg)
{
  const unsigned char* p_current = p_in;
  unsigned int current_len = 0;
  unsigned int total_len = 0;
  while (total_len < inlen)
  {
    current_len = *p_current;
    if (current_len == 3 && !memcmp(p_current + 1, "ftp", 3))
    {
      *p_out = p_current + 1;
      *outlen = 3;
      return SSL_TLSEXT_ERR_OK;
    }
    total_len += current_len + 1;
    p_current += current_len + 1;
  }
  return SSL_TLSEXT_ERR_ALERT_FATAL;
}
