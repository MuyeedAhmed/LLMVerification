static void handle_site(struct vsf_session *p_sess)
{
    static struct mystr s_site_args_str;
    str_split_char(&p_sess->ftp_arg_str, &s_site_args_str, ' ');
    str_upper(&p_sess->ftp_arg_str);

    if (tunable_write_enable && tunable_chmod_enable && str_equal_text(&p_sess->ftp_arg_str, "CHMOD"))
    {
        handle_site_chmod(p_sess, &s_site_args_str);
    }
    else if (str_equal_text(&p_sess->ftp_arg_str, "UMASK"))
    {
        handle_site_umask(p_sess, &s_site_args_str);
    }
    else if (str_equal_text(&p_sess->ftp_arg_str, "HELP"))
    {
        if (tunable_chmod_enable)
        {
            vsf_cmdio_write(p_sess, FTP_SITEHELP, "CHMOD UMASK HELP");
        }
        else
        {
            vsf_cmdio_write(p_sess, FTP_SITEHELP, "UMASK HELP");
        }
    }
    else
    {
        vsf_cmdio_write(p_sess, FTP_BADCMD, "Unknown SITE command.");
    }
}