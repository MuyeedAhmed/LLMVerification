void
vsf_sysutil_tzset(void)
{
  int retval;
  char tzbuf[sizeof("+HHMM!")];
  time_t the_time = time(NULL);
  struct tm* p_tm;
  tzset();
  p_tm = localtime(&the_time);
  if (p_tm == NULL)
  {
    die("localtime");
  }
  /* Set our timezone in the TZ environment variable to cater for the fact
   * that modern glibc does not cache /etc/localtime (which becomes inaccessible
   * when we chroot().
   */
  retval = strftime(tzbuf, sizeof(tzbuf), "%z", p_tm);
  tzbuf[sizeof(tzbuf) - 1] = '\0';
  if (retval == 5)
  {
    /* Static because putenv() does not copy the string. */
    static char envtz[sizeof("TZ=UTC-hh:mm")];
    /* Insert a colon so we have e.g. -05:00 instead of -0500 */
    tzbuf[5] = tzbuf[4];
    tzbuf[4] = tzbuf[3];
    tzbuf[3] = ':';
    /* Invert the sign - we just got the offset _from_ UTC but for TZ, we need
     * the offset _to_ UTC.
     */
    if (tzbuf[0] == '+')
    {
      tzbuf[0] = '-';
    }
    else
    {
      tzbuf[0] = '+';
    }
    snprintf(envtz, sizeof(envtz), "TZ=UTC%s", tzbuf);
    putenv(envtz);
    s_timezone = ((tzbuf[1] - '0') * 10 + (tzbuf[2] - '0')) * 60 * 60;
    s_timezone += ((tzbuf[4] - '0') * 10 + (tzbuf[5] - '0')) * 60;
    if (tzbuf[0] == '-')
    {
      s_timezone *= -1;
    }
  }

  /* Call tzset() again to force the time subsystem to cache the zoneinfo */
  tzset();
}