# Code Citations

## License: unknown
https://github.com/DikuMUDOmnibus/Tartarus/tree/4802a9bc3604a5b14ccadc8064598f23765b203e/jansson/load.c

```
if(c == '-')
        c = lex_get_save(lex, error);

    if(c == '0') {
        c = lex_get_save(lex, error);
        if(isdigit(c)) {
            lex_unget_unsave(lex, c);
            goto out;
```


## License: unknown
https://github.com/svarogg/darkcoin/tree/34d743edc1e8ecab4db4842384d7d284ff75a371/compat/jansson/load.c

```
}

    if(c == '.') {
        c = lex_get(lex, error);
        if(!isdigit(c))
            goto out;
        lex_save(lex, c);

        c = lex_get_save(lex, error);
        while(isdigit(c))
            c
```

