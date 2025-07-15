json_t *json_real(double value)
{
    json_real_t *real;

    if(isnan(value) || isinf(value))
        return NULL;

    real = jsonp_malloc(sizeof(json_real_t));
    if(!real)
        return NULL;
    json_init(&real->json, JSON_REAL);

    real->value = value;
    return &real->json;
}