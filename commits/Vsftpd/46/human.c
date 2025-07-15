int
vsf_sysdep_check_auth(struct mystr* p_user_str,
                      const struct mystr* p_pass_str,
                      const struct mystr* p_remote_host)
{
  int retval;
  pam_item_t item;
  const char* pam_user_name = 0;
  struct pam_conv the_conv =
  {
    &pam_conv_func,
    0
  };
  if (s_pamh != 0)
  {
    bug("vsf_sysdep_check_auth");
  }
  str_copy(&s_pword_str, p_pass_str);
  retval = pam_start(tunable_pam_service_name,
                     str_getbuf(p_user_str), &the_conv, &s_pamh);
  if (retval != PAM_SUCCESS)
  {
    s_pamh = 0;
    return 0;
  }
#ifdef PAM_RHOST
  retval = pam_set_item(s_pamh, PAM_RHOST, str_getbuf(p_remote_host));
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
#endif
#ifdef PAM_TTY
  retval = pam_set_item(s_pamh, PAM_TTY, "ftp");
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
#endif
#ifdef PAM_RUSER
  retval = pam_set_item(s_pamh, PAM_RUSER, str_getbuf(p_user_str));
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
#endif
  retval = pam_authenticate(s_pamh, 0);
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
#ifdef PAM_USER
  retval = pam_get_item(s_pamh, PAM_USER, &item);
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
  pam_user_name = item;
  str_alloc_text(p_user_str, pam_user_name);
#endif
  retval = pam_acct_mgmt(s_pamh, 0);
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
  retval = pam_setcred(s_pamh, PAM_ESTABLISH_CRED);
  if (retval != PAM_SUCCESS)
  {
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
  if (!tunable_session_support)
  {
    /* You're in already! */
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 1;
  }
  /* Must do this BEFORE opening a session for pam_limits to count us */
  vsf_insert_uwtmp(p_user_str, p_remote_host);
  retval = pam_open_session(s_pamh, 0);
  if (retval != PAM_SUCCESS)
  {
    vsf_remove_uwtmp();
    (void) pam_setcred(s_pamh, PAM_DELETE_CRED);
    (void) pam_end(s_pamh, retval);
    s_pamh = 0;
    return 0;
  }
  /* We MUST ensure the PAM session, utmp, wtmp etc. are cleaned up, however
   * we exit.
   */
  vsf_sysutil_set_exit_func(vsf_auth_shutdown);
  /* You're in dude */
  return 1;
}
