static void
limits_init(void)
{
  if (tunable_text_userdb_names)
  {
    vsf_sysutil_set_address_space_limit(VSFTP_AS_LIMIT_LARGE);
  }
  else
  {
    vsf_sysutil_set_address_space_limit(VSFTP_AS_LIMIT);
  }
}
