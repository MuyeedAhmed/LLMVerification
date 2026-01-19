static const char *
symbol_tag (const symbol *sym)
{
    if (sym->tag && *sym->tag)
        return sym->tag;
    return sym->content->type_name ? sym->content->type_name : "end of file";
}
