static const char *
symbol_tag (const symbol *sym)
{
  if (strcmp (sym->tag, "$end") == 0)
  {
    if (internationalization)
      return gettext ("end of file");
    else
      return "end of file";
  }
  return sym->tag;
}
