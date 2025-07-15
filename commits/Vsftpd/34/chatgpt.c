static void
handle_sigchld(void* duff)
{

  struct vsf_sysutil_wait_retval wait_retval = vsf_sysutil_wait();
  (void) duff;
  /* Child died, so we'll do the same! Report it as an error unless the child
   * exited normally with zero exit code
   */
  if (vsf_sysutil_retval_is_error(vsf_sysutil_wait_get_retval(&wait_retval)) ||
      !vsf_sysutil_wait_exited_normally(&wait_retval) ||
      vsf_sysutil_wait_get_exitcode(&wait_retval) != 0)
  { 
    die("child died");
  }
  else
  {
    vsf_sysutil_exit(0);
  }
}