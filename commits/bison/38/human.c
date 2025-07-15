/** Return an allocated string that represents the %define directive
    that performs the assignment.

    @param assignment "VAR", or "VAR=VAL".
    @param value      default value if VAL \a assignment has no '='.

    For instance:
    "foo", NULL      => "%define foo"
    "foo", "baz"     => "%define foo baz"
    "foo=bar", NULL  => "%define foo bar"
    "foo=bar", "baz" => "%define foo bar"
    "foo=", NULL     => "%define foo"
    "foo=", "baz"    => "%define foo"
 */

 static
 char *
 define_directive (char const *assignment,
                   muscle_kind kind,
                   char const *value)
 {
   char *eq = strchr (assignment, '=');
   char const *fmt
     = eq || !value || !*value ? "%%define %s"
     : kind == muscle_code     ? "%%define %s {%s}"
     : kind == muscle_string   ? "%%define %s \"%s\""
     :                           "%%define %s %s";
   char *res = xmalloc (strlen (fmt) + strlen (assignment)
                        + (value ? strlen (value) : 0));
   sprintf (res, fmt, assignment, value);
   eq = strchr (res, '=');
   if (eq)
     *eq = eq[1] ? ' ' : '\0';
   return res;
 }