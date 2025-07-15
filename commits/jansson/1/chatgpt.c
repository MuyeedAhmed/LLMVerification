static void lex_scan_number(lex_t *lex, char c, json_error_t *error)
{
    const char *saved_text;
    char *end;

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

        errno = 0; // Reset errno before conversion
        lex->value.integer = strtol(saved_text, &end, 10);
        if (errno == ERANGE) {
            error_set(error, lex, "integer overflow or underflow");
            goto out;
        }
        assert(end == saved_text + lex->saved_text.length);

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

    errno = 0; // Reset errno before conversion
    lex->value.real = strtod(saved_text, &end);
    if (errno == ERANGE) {
        error_set(error, lex, "real number overflow or underflow");
        goto out;
    }
    assert(end == saved_text + lex->saved_text.length);

out:
    return;
}