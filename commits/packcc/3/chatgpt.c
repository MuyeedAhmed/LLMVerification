#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
typedef enum bool_tag {
    FALSE = 0,
    TRUE
} bool_t;

static bool_t unescape_string(char *str) {
    bool_t b = TRUE;
    size_t i, j;
    for (j = 0, i = 0; str[i]; i++) {
        if (str[i] == '\\') {
            i++;
            switch (str[i]) {
            case '\0': str[j++] = '\\'; str[j] = '\0'; return FALSE;
            case '\\': str[j++] = '\\'; break; // Support escaped backslash
            case '0': str[j++] = '\x00'; break;
            case 'a': str[j++] = '\x07'; break;
            case 'b': str[j++] = '\x08'; break;
            case 'f': str[j++] = '\x0c'; break;
            case 'n': str[j++] = '\x0a'; break;
            case 'r': str[j++] = '\x0d'; break;
            case 't': str[j++] = '\x09'; break;
            case 'v': str[j++] = '\x0b'; break;
            case 'x': // Handle hexadecimal escape sequences
                if (isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {
                    char hex[3] = {str[i + 1], str[i + 2], '\0'};
                    str[j++] = (char)strtol(hex, NULL, 16);
                    i += 2;
                } else {
                    str[j++] = '\\';
                    str[j++] = 'x';
                    b = FALSE;
                }
                break;
            default: str[j++] = '\\'; str[j++] = str[i]; break;
            }
        } else {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
    return b;
}