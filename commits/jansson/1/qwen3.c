static void lex_scan_number(lex_t *lex, char c, json_error_t *error)
{
    const char *saved_text;
    char *end;
    long long_value;
    double real_value;
    char *endptr;

    lex->token = TOKEN_INVALID;

    if(c == '-')
        c = lex_get_save(lex, error);

    if(c == '0') {
        c = lex_get_save(lex, error);
        if(isdigit(c)) {
            lex_unget_unsave(lex, c);
            error_set(error, lex, "leading zero not allowed");
            return;
        }
    }
    else {
        c = lex_get_save(lex, error);
        while(isdigit(c))
            c = lex_get_save(lex, error);
    }

    if(c != '.' && c != 'E' && c != 'e') {
        lex_unget_unsave(lex, c);
        lex->token = TOKEN_INTEGER;

        saved_text = strbuffer_value(&lex->saved_text);
        errno = 0;
        long_value = strtoll(saved_text, &endptr, 10);
        if(errno == ERANGE) {
            error_set(error, lex, "integer overflow");
            return;
        }
        if(endptr == saved_text) {
            error_set(error, lex, "invalid integer");
            return;
        }
        lex->value.integer = long_value;
        return;
    }

    if(c == '.') {
        c = lex_get(lex, error);
        if(!isdigit(c)) {
            error_set(error, lex, "invalid number");
            return;
        }
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
            error_set(error, lex, "invalid number");
            return;
        }

        c = lex_get_save(lex, error);
        while(isdigit(c))
            c = lex_get_save(lex, error);
    }

    lex_unget_unsave(lex, c);
    lex->token = TOKEN_REAL;

    saved_text = strbuffer_value(&lex->saved_text);
    errno = 0;
    real_value = strtod(saved_text, &endptr);
    if(errno == ERANGE) {
        error_set(error, lex, "real overflow");
        return;
    }
    if(endptr == saved_text) {
        error_set(error, lex, "invalid real");
        return;
    }
    lex->value.real = real_value;
}
