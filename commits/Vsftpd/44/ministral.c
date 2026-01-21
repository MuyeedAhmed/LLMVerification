void
ssl_init(struct vsf_session* p_sess)
{
  if (!ssl_inited)
  {
    SSL_CTX* p_ctx;
    long options;
    int verify_option = 0;
    SSL_library_init();
    p_ctx = SSL_CTX_new(SSLv23_server_method());
    if (p_ctx == NULL)
    {
      die("SSL: could not allocate SSL context");
    }
    options = SSL_OP_ALL;
    if (!tunable_sslv2)
    {
      options |= SSL_OP_NO_SSLv2;
    }
    if (!tunable_sslv3)
    {
      options |= SSL_OP_NO_SSLv3;
    }
    if (!tunable_tlsv1)
    {
      options |= SSL_OP_NO_TLSv1;
    }
    SSL_CTX_set_options(p_ctx, options);
    if (tunable_rsa_cert_file)
    {
      const char* p_key = tunable_rsa_private_key_file;
      if (!p_key)
      {
        p_key = tunable_rsa_cert_file;
      }
      if (SSL_CTX_use_certificate_chain_file(p_ctx, tunable_rsa_cert_file) != 1)
      {
        die("SSL: cannot load RSA certificate");
      }
      if (SSL_CTX_use_PrivateKey_file(p_ctx, p_key, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load RSA private key");
      }
    }
    if (tunable_dsa_cert_file)
    {
      const char* p_key = tunable_dsa_private_key_file;
      if (!p_key)
      {
        p_key = tunable_dsa_cert_file;
      }
      if (SSL_CTX_use_certificate_chain_file(p_ctx, tunable_dsa_cert_file) != 1)
      {
        die("SSL: cannot load DSA certificate");
      }
      if (SSL_CTX_use_PrivateKey_file(p_ctx, p_key, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load DSA private key");
      }
    }
    if (tunable_ssl_ciphers &&
        SSL_CTX_set_cipher_list(p_ctx, tunable_ssl_ciphers) != 1)
    {
      die("SSL: could not set cipher list");
    }
    if (RAND_status() != 1)
    {
      die("SSL: RNG is not seeded");
    }
    if (tunable_ssl_request_cert)
    {
      verify_option |= SSL_VERIFY_PEER;
      if (tunable_ca_certs_file)
      {
        if (!SSL_CTX_load_verify_locations(p_ctx, tunable_ca_certs_file, NULL))
        {
          die("SSL: could not load verify file");
        }
      }
      if (tunable_ca_certs_file || tunable_ca_certs_dir)
      {
        STACK_OF(X509_NAME) *ca_names = NULL;
        X509_STORE *ca_store = SSL_CTX_get_cert_store(p_ctx);
        if (ca_store)
        {
          ca_names = SSL_CTX_get_client_CA_list(p_ctx);
          if (!ca_names)
          {
            ca_names = sk_X509_NAME_new_null();
            if (ca_names)
            {
              if (tunable_ca_certs_file)
              {
                X509_STORE_add_dir(ca_store, tunable_ca_certs_dir);
              }
              if (tunable_ca_certs_dir)
              {
                X509_STORE_add_dir(ca_store, tunable_ca_certs_dir);
              }
              SSL_CTX_set_client_CA_list(p_ctx, ca_names);
            }
          }
        }
      }
    }
    if (verify_option)
    {
      SSL_CTX_set_verify(p_ctx, verify_option, ssl_verify_callback);
    }
    {
      char* p_ctx_id = "vsftpd";
      SSL_CTX_set_session_id_context(p_ctx, (void*) p_ctx_id,
                                     vsf_sysutil_strlen(p_ctx_id));
    }
    p_sess->p_ssl_ctx = p_ctx;
    ssl_inited = 1;
  }
}
