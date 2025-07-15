void
vsf_secutil_change_credentials(const struct mystr* p_user_str,
                               const struct mystr* p_dir_str,
                               const struct mystr* p_ext_dir_str,
                               unsigned int caps, unsigned int options)
{
    struct vsf_sysutil_user* p_user;

    if (!vsf_sysutil_running_as_root()) {
        bug("vsf_secutil_change_credentials: not running as root");
    }

    p_user = str_getpwnam(p_user_str);
    if (p_user == 0) {
        die2("cannot locate user entry:", str_getbuf(p_user_str));
    }

    struct mystr dir_str = INIT_MYSTR;

    if (p_dir_str == 0 || str_isempty(p_dir_str)) {
        str_alloc_text(&dir_str, vsf_sysutil_user_get_homedir(p_user));
    } else {
        str_copy(&dir_str, p_dir_str);
    }

    if (options & VSF_SECUTIL_OPTION_USE_GROUPS) {
        vsf_sysutil_initgroups(p_user);
    } else {
        vsf_sysutil_clear_supp_groups();
    }

    int saved_euid = 0;
    int saved_egid = 0;
    int retval;

    if (options & VSF_SECUTIL_OPTION_CHANGE_EUID) {
        saved_euid = vsf_sysutil_geteuid();
        saved_egid = vsf_sysutil_getegid();
        vsf_sysutil_setegid(p_user);
        vsf_sysutil_seteuid(p_user);
    }

    retval = str_chdir(&dir_str);
    if (retval != 0) {
        die2("cannot change directory:", str_getbuf(&dir_str));
    }

    if (p_ext_dir_str && !str_isempty(p_ext_dir_str)) {
        retval = str_chdir(p_ext_dir_str);
        if (retval != 0 && !(options & VSF_SECUTIL_OPTION_CHROOT)) {
            retval = 0;
        }
    }

    if (retval != 0) {
        die2("cannot change directory:", str_getbuf(p_ext_dir_str));
    }

    if (options & VSF_SECUTIL_OPTION_CHANGE_EUID) {
        vsf_sysutil_seteuid_numeric(saved_euid);
        vsf_sysutil_setegid_numeric(saved_egid);
    }

    if (options & VSF_SECUTIL_OPTION_CHROOT) {
        if (vsf_sysutil_is_writable_directory(".")) {
            die("chroot() to a directory writable by the current user is not allowed.");
        }
        vsf_sysutil_chroot(".");

        if (vsf_sysutil_is_writable_directory("/")) {
            die("Root directory inside chroot() is writable by the current user.");
        }
    }

    str_free(&dir_str);

    if (options & VSF_SECUTIL_OPTION_NO_FDS) {
        vsf_sysutil_set_no_fds();
    }

    if (caps) {
        if (!vsf_sysdep_has_capabilities()) {
            return;
        }
        if (!vsf_sysdep_has_capabilities_as_non_root()) {
            vsf_sysdep_adopt_capabilities(caps);
            return;
        }
        vsf_sysdep_keep_capabilities();
    }

    vsf_sysutil_setgid(p_user);
    vsf_sysutil_setuid(p_user);

    if (caps) {
        vsf_sysdep_adopt_capabilities(caps);
    }

    if (options & VSF_SECUTIL_OPTION_NO_PROCS) {
        vsf_sysutil_set_no_procs();
    }
}