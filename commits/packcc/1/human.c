static const char *escape_character(char ch, char (*buf)[5]) {
    switch (ch) {
    case '\x00': strncpy(*buf, "\\0", 5); break;
    case '\x07': strncpy(*buf, "\\a", 5); break;
    case '\x08': strncpy(*buf, "\\b", 5); break;
    case '\x0c': strncpy(*buf, "\\f", 5); break;
    case '\x0a': strncpy(*buf, "\\n", 5); break;
    case '\x0d': strncpy(*buf, "\\r", 5); break;
    case '\x09': strncpy(*buf, "\\t", 5); break;
    case '\x0b': strncpy(*buf, "\\v", 5); break;
    case '\\':  strncpy(*buf, "\\\\", 5); break;
    case '\'':  strncpy(*buf, "\\\'", 5); break;
    case '\"':  strncpy(*buf, "\\\"", 5); break;
    default:
        if (ch >= '\x20' && ch < '\x7f')
            snprintf(*buf, 5, "%c", ch);
        else
            snprintf(*buf, 5, "\\x%02x", (unsigned char)ch);
    }
    (*buf)[4] = '\0';
    return *buf;
}