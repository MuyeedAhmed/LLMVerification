static bool_t parse(context_t *ctx) {
    parse_file_(ctx);
    make_rulehash(ctx);

    if (ctx->rules.len == 0) {
        print_error("No rules defined in the input file.\n");
        return FALSE;
    }

    {
        size_t i;
        for (i = 0; i < ctx->rules.len; i++) {
            node_rule_t *const rule = &ctx->rules.buf[i]->data.rule;
            if (rule->ref < 0) {
                print_error("Undefined rule: %s\n", rule->name);
                ctx->errnum++;
            }
            link_references(ctx, rule->expr);
        }
    }

    if (ctx->rules.len > 0 && ctx->rules.buf[0] != NULL) {
        mark_rules_if_used(ctx, ctx->rules.buf[0]);
    } else {
        print_error("No valid rules to process.\n");
        return FALSE;
    }

    {
        size_t i, j;
        for (i = 0, j = 0; i < ctx->rules.len; i++) {
            if (!ctx->rules.buf[i]->data.rule.used) {
                destroy_node(ctx->rules.buf[i]);
            } else {
                ctx->rules.buf[j++] = ctx->rules.buf[i];
            }
        }
        ctx->rules.len = j;
    }

    {
        size_t i;
        for (i = 0; i < ctx->rules.len; i++) {
            const node_rule_t *const rule = &ctx->rules.buf[i]->data.rule;
            verify_variables(ctx, rule->expr, NULL);
            verify_captures(ctx, rule->expr, NULL);
        }
    }

    if (ctx->opts.debug) {
        size_t i;
        for (i = 0; i < ctx->rules.len; i++) {
            dump_node(ctx, ctx->rules.buf[i], 0);
        }
        dump_options(ctx);
    }

    return (ctx->errnum == 0) ? TRUE : FALSE;
}