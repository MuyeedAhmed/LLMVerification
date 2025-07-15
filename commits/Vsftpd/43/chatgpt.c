enum EVSFPrivopLoginResult
vsf_privop_do_login(struct vsf_session* p_sess, const struct mystr* p_pass_str)
{
    enum EVSFPrivopLoginResult result = handle_login(p_sess, &p_sess->user_str, p_pass_str);
    vsf_log_start_entry(p_sess, kVSFLogEntryLogin);
    if (result == kVSFLoginFail)
    {
        vsf_sysutil_sleep((double)tunable_delay_failed_login); // Add fail delay
        vsf_log_do_log(p_sess, 0);
    }
    else
    {
        vsf_log_do_log(p_sess, 1);
        if (tunable_delay_successful_login)
        {
            vsf_sysutil_sleep((double)tunable_delay_successful_login);
        }
    }
    return result;
}