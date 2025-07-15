static void
limits_init(void)
{
  unsigned long limit = VSFTP_AS_LIMIT;
  if (tunable_text_userdb_names)
  {
    /* Turns out, LDAP lookups for lots of userid -> name mappings can really
     * bloat memory usage.
     */
    limit *= 3;
  }
  vsf_sysutil_set_address_space_limit(limit);
}