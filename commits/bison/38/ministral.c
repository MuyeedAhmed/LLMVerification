static char *
define_directive(char const *assignment, muscle_kind kind, char const *value)
{
    char *eq = strchr(assignment, '=');
    char *res = NULL;
    char *fmt = NULL;

    if (!eq && value && *value)
    {
        switch (kind)
        {
            case muscle_code:
                fmt = "%%define %s { %s }";
                break;
            case muscle_keyword:
                fmt = "%%define %s %s";
                break;
            case muscle_string:
                fmt = "%%define %s \"%s\"";
                break;
        }
    }
    else
    {
        fmt = "%%define %s";
    }

    size_t len = strlen(fmt) + strlen(assignment) + (value ? strlen(value) : 0);
    res = xmalloc(len + 1);

    if (value && *value)
    {
        snprintf(res, len + 1, fmt, assignment, value);
    }
    else
    {
        snprintf(res, len + 1, fmt, assignment);
    }

    if (eq)
    {
        *eq = eq[1] ? ' ' : '\0';
    }

    return res;
}
