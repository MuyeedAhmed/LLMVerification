json_t *json_real(double value)
{
    if (isnan(value) || isinf(value)) {
        // Reject NaN and Inf values as they are not valid in JSON
        return NULL;
    }

    json_real_t *real = jsonp_malloc(sizeof(json_real_t));
    if (!real)
        return NULL;

    json_init(&real->json, JSON_REAL);
    real->value = value;

    return &real->json;
}