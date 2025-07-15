static int32_t decode_unicode_escape(const char *str)
{
    int i;
    int32_t value = 0;

    assert(str[0] == 'u');

    for(i = 1; i <= 4; i++) {
        char c = str[i];
        value <<= 4;
        if(isdigit(c))
            value += c - '0';
        else if(islower(c))
            value += c - 'a' + 10;
        else if(isupper(c))
            value += c - 'A' + 10;
        else
            assert(0);
    }

    return value;
}