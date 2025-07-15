uniqstr
symbol_list_n_type_name_get (symbol_list *l, int n)
{
  l = symbol_list_n_get(l, n);
  if (!l)
    return NULL;
  aver(l->content_type == SYMLIST_SYMBOL);
  return l->content.sym->type_name;
}