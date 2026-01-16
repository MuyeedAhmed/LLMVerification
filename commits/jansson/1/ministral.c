static void lex_scan_number(lex_t *lex, char c, json_error_t *error)
{
    const char *saved_text;
    char *end;
    long long intval;
    double realval;

    lex->token = TOKEN_INVALID;

    if(c == '-')
        c = lex_get_save(lex, error);

    if(c == '0') {
        c = lex_get_save(lex, error);
        if(isdigit(c)) {
            lex_unget_unsave(lex, c);
            goto out;
        }
    }
    else /* c != '0' */ {
        c = lex_get_save(lex, error);
        while(isdigit(c))
            c = lex_get_save(lex, error);
    }

    if(c != '.' && c != 'E' && c != 'e') {
        lex_unget_unsave(lex, c);
        lex->token = TOKEN_INTEGER;

        saved_text = strbuffer_value(&lex->saved_text);
        errno = 0;
        intval = strtoll(saved_text, &end, 10);

        if (end == saved_text || *end != '\0' || errno == ERANGE) {
            error_set(error, lex, "invalid integer");
            goto out;
        }

        lex->value.integer = intval;
        return;
    }

    if(c == '.') {
        c = lex_get(lex, error);
        if(!isdigit(c))
            goto out;
        lex_save(lex, c);

        c = lex_get_save(lex, error);
        while(isdigit(c))
            c = lex_get_save(lex, error);
    }

    if(c == 'E' || c == 'e') {
        c = lex_get_save(lex, error);
        if(c == '+' || c == '-')
            c = lex_get_save(lex, error);

        if(!isdigit(c)) {
            lex_unget_unsave(lex, c);
            goto out;
        }

        c = lex_get_save(lex, error);
        while(isdigit(c))
            c = lex_get_save(lex, error);
    }

    lex_unget_unsave(lex, c);
    lex->token = TOKEN_REAL;

    saved_text = strbuffer_value(&lex->saved_text);
    errno = 0;
    realval = strtod(saved_text, &end);

    if (end == saved_text || *end != '\0' || errno == ERANGE) {
        error_set(error, lex, "invalid real number");
        goto out;
    }

    lex->value.real = realval;
    return;

out:
    return;
}
