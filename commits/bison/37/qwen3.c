void
symbol_make_alias (symbol *sym, symbol *str, location loc)
{
  if (sym->content->class == nterm_sym)
    complain (&loc, Wother, _("nonterminal %s cannot be aliased"), sym->tag);
  else if (str->alias)
    complain (&loc, Wother,
              _("symbol %s used more than once as a literal string"), str->tag);
  else if (sym->alias)
    complain (&loc, Wother,
              _("symbol %s given more than one literal string"), sym->tag);
  else
    {
      symbol_merge_properties (sym, str);
      sym_content_free (str->content);
      str->content = sym->content;
      str->content->symbol = str;
      str->is_alias = true;
      str->alias = sym;
      sym->alias = str;
    }
}
