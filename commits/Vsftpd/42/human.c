static void
handle_upload_common(struct vsf_session* p_sess, int is_append, int is_unique)
{
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  static struct mystr s_filename;
  struct mystr* p_filename;
  struct vsf_transfer_ret trans_ret;
  int new_file_fd;
  int remote_fd;
  int success = 0;
  int created = 0;
  int do_truncate = 0;
  filesize_t offset = p_sess->restart_pos;
  p_sess->restart_pos = 0;
  if (!data_transfer_checks_ok(p_sess))
  {
    return;
  }
  resolve_tilde(&p_sess->ftp_arg_str, p_sess);
  p_filename = &p_sess->ftp_arg_str;
  if (is_unique)
  {
    get_unique_filename(&s_filename, p_filename);
    p_filename = &s_filename;
  }
  vsf_log_start_entry(p_sess, kVSFLogEntryUpload);
  str_copy(&p_sess->log_str, &p_sess->ftp_arg_str);
  prepend_path_to_filename(&p_sess->log_str);
  if (!vsf_access_check_file(p_filename))
  {
    vsf_cmdio_write(p_sess, FTP_NOPERM, "Permission denied.");
    return;
  }
  /* NOTE - actual file permissions will be governed by the tunable umask */
  /* XXX - do we care about race between create and chown() of anonymous
   * upload?
   */
  if (is_unique || (p_sess->is_anonymous && !tunable_anon_other_write_enable))
  {
    new_file_fd = str_create(p_filename);
  }
  else
  {
    /* For non-anonymous, allow open() to overwrite or append existing files */
    new_file_fd = str_create_append(p_filename);
    if (!is_append && offset == 0)
    {
      do_truncate = 1;
    }
  }
  if (vsf_sysutil_retval_is_error(new_file_fd))
  {
    vsf_cmdio_write(p_sess, FTP_UPLOADFAIL, "Could not create file.");
    return;
  }
  created = 1;
  vsf_sysutil_fstat(new_file_fd, &s_p_statbuf);
  if (vsf_sysutil_statbuf_is_regfile(s_p_statbuf))
  {
    /* Now deactive O_NONBLOCK, otherwise we have a problem on DMAPI filesystems
     * such as XFS DMAPI.
     */
    vsf_sysutil_deactivate_noblock(new_file_fd);
  }
  /* Are we required to chown() this file for security? */
  if (p_sess->is_anonymous && tunable_chown_uploads)
  {
    vsf_sysutil_fchmod(new_file_fd, tunable_chown_upload_mode);
    if (tunable_one_process_model)
    {
      vsf_one_process_chown_upload(p_sess, new_file_fd);
    }
    else
    {
      vsf_two_process_chown_upload(p_sess, new_file_fd);
    }
  }
  /* Are we required to lock this file? */
  if (tunable_lock_upload_files)
  {
    vsf_sysutil_lock_file_write(new_file_fd);
  }
  /* Must truncate the file AFTER locking it! */
  if (do_truncate)
  {
    vsf_sysutil_ftruncate(new_file_fd);
    vsf_sysutil_lseek_to(new_file_fd, 0);
  }
  if (!is_append && offset != 0)
  {
    /* XXX - warning, allows seek past end of file! Check for seek > size? */
    /* XXX - also, currently broken as the O_APPEND flag will always write
     * at the end of file. No known complaints yet; can easily fix if one
     * comes in.
     */
    vsf_sysutil_lseek_to(new_file_fd, offset);
  }
  if (is_unique)
  {
    struct mystr resp_str = INIT_MYSTR;
    str_alloc_text(&resp_str, "FILE: ");
    str_append_str(&resp_str, p_filename);
    remote_fd = get_remote_transfer_fd(p_sess, str_getbuf(&resp_str));
    str_free(&resp_str);
  }
  else
  {
    remote_fd = get_remote_transfer_fd(p_sess, "Ok to send data.");
  }
  if (vsf_sysutil_retval_is_error(remote_fd))
  {
    goto port_pasv_cleanup_out;
  }
  if (tunable_ascii_upload_enable && p_sess->is_ascii)
  {
    trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
                                            new_file_fd, 1, 1);
  }
  else
  {
    trans_ret = vsf_ftpdataio_transfer_file(p_sess, remote_fd,
                                            new_file_fd, 1, 0);
  }
  if (vsf_ftpdataio_dispose_transfer_fd(p_sess) != 1 && trans_ret.retval == 0)
  {
    trans_ret.retval = -2;
  }
  p_sess->transfer_size = trans_ret.transferred;
  if (trans_ret.retval == 0)
  {
    success = 1;
    vsf_log_do_log(p_sess, 1);
  }
  if (trans_ret.retval == -1)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDFILE, "Failure writing to local file.");
  }
  else if (trans_ret.retval == -2 || p_sess->abor_received)
  {
    vsf_cmdio_write(p_sess, FTP_BADSENDNET, "Failure reading network stream.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_TRANSFEROK, "Transfer complete.");
  }
  check_abor(p_sess);
port_pasv_cleanup_out:
  port_cleanup(p_sess);
  pasv_cleanup(p_sess);
  if (tunable_delete_failed_uploads && created && !success)
  {
    str_unlink(p_filename);
  }
  vsf_sysutil_close(new_file_fd);
}