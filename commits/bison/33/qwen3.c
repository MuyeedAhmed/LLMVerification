static const char *
symbol_tag (const symbol *sym)
{
  if (sym->content->number == undeftoken->content->number)
    return _("end of file");
  else
    return sym->tag;
}
