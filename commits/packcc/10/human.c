static bool_t parse(context_t *ctx) {
    parse_file_(ctx);
    make_rulehash(ctx);
    {
        size_t i;
        for (i = 0; i < ctx->rules.len; i++) {
            node_rule_t *const rule = &ctx->rules.buf[i]->data.rule;
            if (rule->ref < 0) {
                print_error(
                    "%s:" FMT_LU ":" FMT_LU ": Multiple definitions of rule: '%s'\n",
                    rule->fpos.path, (ulong_t)(rule->fpos.line + 1), (ulong_t)(rule->fpos.col + 1),
                    rule->name
                );
                ctx->errnum++;
                continue;
            }
            link_references(ctx, rule->expr);
        }
    }
    if (ctx->rules.len > 0)
        mark_rules_if_used(ctx, ctx->rules.buf[0]);
    {
        size_t i;
        for (i = 0; i < ctx->rules.len; i++) {
            if (!ctx->rules.buf[i]->data.rule.used)
                unreference_rules_from_unused_rule(ctx, ctx->rules.buf[i]);
        }
    }
    {
        size_t i, j;
        for (i = 0, j = 0; i < ctx->rules.len; i++) {
            if (!ctx->rules.buf[i]->data.rule.used)
                destroy_node(ctx->rules.buf[i]);
            else
                ctx->rules.buf[j++] = ctx->rules.buf[i];
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