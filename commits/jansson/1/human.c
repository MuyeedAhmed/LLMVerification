static int lex_scan_number(lex_t *lex, char c, json_error_t *error)
{
    const char *saved_text;
    char *end;
    double value;

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
        long value;

        lex_unget_unsave(lex, c);

        saved_text = strbuffer_value(&lex->saved_text);
        value = strtol(saved_text, &end, 10);
        assert(end == saved_text + lex->saved_text.length);

        if((value == LONG_MAX && errno == ERANGE) || value > INT_MAX) {
            error_set(error, lex, "too big integer");
            goto out;
        }
        else if((value == LONG_MIN && errno == ERANGE) || value < INT_MIN) {
            error_set(error, lex, "too big negative integer");
            goto out;
        }

        lex->token = TOKEN_INTEGER;
        lex->value.integer = (int)value;
        return 0;
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

    saved_text = strbuffer_value(&lex->saved_text);
    value = strtod(saved_text, &end);
    assert(end == saved_text + lex->saved_text.length);

    if(value == 0 && errno == ERANGE) {
        error_set(error, lex, "real number underflow");
        goto out;
    }

    /* Cannot test for +/-HUGE_VAL because the HUGE_VAL constant is
       only defined in C99 mode. So let's trust in sole errno. */
    else if(errno == ERANGE) {
        error_set(error, lex, "real number overflow");
        goto out;
    }

    lex->token = TOKEN_REAL;
    lex->value.real = value;
    return 0;

out:
    return -1;
}