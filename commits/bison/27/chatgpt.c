void
item_print (item_number *item, rule const *previous_rule, FILE *out)
{
  rule const *r = item_rule (item);
  rule_lhs_print (r, previous_rule ? previous_rule->lhs : NULL, out);

  for (item_number *sp = r->rhs; sp < item; sp++)
    fprintf (out, " %s", symbols[*sp]->tag);
  fprintf (out, " %s", dot);
  if (0 <= *r->rhs)
    for (item_number *sp = item; 0 <= *sp; ++sp)
      fprintf (out, " %s", symbols[*sp]->tag);
  else
    fprintf (out, " %%empty.");
}