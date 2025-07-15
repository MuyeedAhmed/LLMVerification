static json_t *pack_object(scanner_t *s, va_list *ap)
{
    json_t *object = json_object();
    next_token(s);

    while (token(s) != '}') {
        char *key;
        size_t len;
        int ours;
        json_t *value;

        if (!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            goto error;
        }

        if (token(s) != 's') {
            set_error(s, "<format>", "Expected format 's', got '%c'", token(s));
            goto error;
        }

        key = read_string(s, ap, "object key", &len, &ours);
        if (!key)
            goto error;

        next_token(s);

        value = pack(s, ap);
        if (!value) {
            if (ours)
                jsonp_free(key); // Free the key before jumping to error
            goto error;
        }

        if (json_object_set_new_nocheck(object, key, value)) {
            if (ours)
                jsonp_free(key); // Free the key before jumping to error
            json_decref(value); // Decrement reference count to avoid use-after-free
            set_error(s, "<internal>", "Unable to add key \"%s\"", key);
            goto error;
        }

        if (ours)
            jsonp_free(key);

        next_token(s);
    }

    return object;

error:
    json_decref(object); // Free the object to avoid memory leaks
    return NULL;
}