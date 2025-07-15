json_t *json_load_file(const char *path, json_error_t *error)
{
    json_t *result;
    FILE *fp;

    error_init(error);

    fp = fopen(path, "r");
    if(!fp)
    {
        error_set(error, NULL, "unable to open %s: %s",
                  path, strerror(errno));
        return NULL;
    }

    result = json_loadf(fp, error);

    fclose(fp);
    return result;
}