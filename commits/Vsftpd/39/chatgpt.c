static void
handle_user_command(struct vsf_session* p_sess)
{
    int is_anon = 1;
    str_copy(&p_sess->user_str, &p_sess->ftp_arg_str);
    str_upper(&p_sess->ftp_arg_str);
    if (!str_equal_text(&p_sess->ftp_arg_str, "FTP") &&
        !str_equal_text(&p_sess->ftp_arg_str, "ANONYMOUS"))
    {
        is_anon = 0;
    }
    if (!tunable_local_enable && !is_anon)
    {
        vsf_cmdio_write(
            p_sess, FTP_LOGINERR, "This FTP server is anonymous only.");
        str_empty(&p_sess->user_str);
        return;
    }
    if (is_anon && p_sess->control_use_ssl && !tunable_allow_anon_ssl &&
        !tunable_force_anon_logins_ssl)
    {
        vsf_cmdio_write(
            p_sess, FTP_LOGINERR, "Anonymous sessions may not use encryption.");
        str_empty(&p_sess->user_str);
        return;
    }
    if (tunable_ssl_enable && !is_anon && !p_sess->control_use_ssl &&
        tunable_force_local_logins_ssl)
    {
        vsf_cmdio_write(
            p_sess, FTP_LOGINERR, "Non-anonymous sessions must use encryption.");
        str_empty(&p_sess->user_str);
        return;
    }
    if (tunable_ssl_enable && is_anon && !p_sess->control_use_ssl &&
        tunable_force_anon_logins_ssl)
    { 
        vsf_cmdio_write(
            p_sess, FTP_LOGINERR, "Anonymous sessions must use encryption.");
        str_empty(&p_sess->user_str);
        return;
    }

    if (!is_anon && !p_sess->control_use_ssl && tunable_force_local_logins_ssl)
    {
        vsf_cmdio_write(p_sess, FTP_LOGINERR, "Encryption is required for USER command.");
        str_empty(&p_sess->user_str);
        vsf_sysutil_exit(0);
    }

    if (tunable_userlist_enable)
    {
        int located = str_contains_line(&p_sess->userlist_str, &p_sess->user_str);
        if ((located && tunable_userlist_deny) ||
            (!located && !tunable_userlist_deny))
        {
            check_login_delay();
            vsf_cmdio_write(p_sess, FTP_LOGINERR, "Permission denied.");
            check_login_fails(p_sess);
            str_empty(&p_sess->user_str);
            return;
        }
    }
    if (is_anon && tunable_no_anon_password)
    {
        str_alloc_text(&p_sess->ftp_arg_str, "<no password>");
        handle_pass_command(p_sess);
    }
    else
    {
        vsf_cmdio_write(p_sess, FTP_GIVEPWORD, "Please specify the password.");
    }
}