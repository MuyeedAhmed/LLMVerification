static char *
define_directive (char const *assignment, muscle_kind kind, char const *value)
{
  char *eq = strchr(assignment, '=');
  char const *fmt = NULL;

  switch (kind)
  {
    case muscle_code:
      fmt = !eq && value && *value ? "%%define %s {%s}" : "%%define %s";
      break;
    case muscle_keyword:
      fmt = !eq && value && *value ? "%%define %s %s" : "%%define %s";
      break;
    case muscle_string:
      fmt = !eq && value && *value ? "%%define %s \"%s\"" : "%%define %s";
      break;
  }

  char *res = xmalloc(strlen(fmt) + strlen(assignment) + (value ? strlen(value) : 0));
  sprintf(res, fmt, assignment, value);
  eq = strchr(res, '=');
  if (eq)
    *eq = eq[1] ? ' ' : '\0';
  return res;
}