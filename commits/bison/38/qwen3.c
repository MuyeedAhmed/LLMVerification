static char *
define_directive (char const *assignment, muscle_kind kind, char const *value)
{
  char *eq = strchr (assignment, '=');
  char const *fmt = !eq && value && *value ? "%%define %s %s" : "%%define %s";
  char *res = xmalloc (strlen (fmt) + strlen (assignment)
                       + (value ? strlen (value) : 0));
  sprintf (res, fmt, assignment, value);
  eq = strchr (res, '=');
  if (eq)
    *eq = eq[1] ? ' ' : '\0';
  return res;
}
