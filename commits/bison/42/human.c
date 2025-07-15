/*--------------------------------------------------------------.
| Get the data type (alternative in the union) of the value for |
| symbol N in symbol list L.                                    |
`--------------------------------------------------------------*/

uniqstr
symbol_list_n_type_name_get (symbol_list *l, int n)
{
  return symbol_list_n_get (l, n)->content.sym->content->type_name;
}