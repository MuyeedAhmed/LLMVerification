json_t *json_load_file(const char *path, json_error_t *error)
{
    json_t *result = NULL;
    FILE *fp = fopen(path, "r");

    if (!fp) {
        if (error) {
            error_set(error, NULL, "unable to open file '%s': %s",
                      path, strerror(errno));
        }
        return NULL;
    }

    result = json_loadf(fp, error);
    fclose(fp);

    return result;
}