void
handle_feat(struct vsf_session* p_sess)
{
  vsf_cmdio_write_hyphen(p_sess, FTP_FEAT, "Features:");
  if (tunable_ssl_enable)
  {
    if (tunable_sslv2_enable || tunable_sslv3_enable)
    {
      vsf_cmdio_write_raw(p_sess, " AUTH SSL\r\n");
    }
    vsf_cmdio_write_raw(p_sess, " AUTH TLS\r\n");
  }
  if (tunable_port_enable)
  {
    vsf_cmdio_write_raw(p_sess, " EPRT\r\n");
  }
  if (tunable_pasv_enable)
  {
    vsf_cmdio_write_raw(p_sess, " EPSV\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " MDTM\r\n");
  if (tunable_pasv_enable)
  {
    vsf_cmdio_write_raw(p_sess, " PASV\r\n");
  }
  if (tunable_ssl_enable)
  {
    vsf_cmdio_write_raw(p_sess, " PBSZ\r\n");
    vsf_cmdio_write_raw(p_sess, " PROT\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " REST STREAM\r\n");
  vsf_cmdio_write_raw(p_sess, " SIZE\r\n");
  vsf_cmdio_write_raw(p_sess, " TVFS\r\n");
  vsf_cmdio_write_raw(p_sess, " UTF8\r\n");
  vsf_cmdio_write(p_sess, FTP_FEAT, "End");
}