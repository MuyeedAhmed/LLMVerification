static void
print_core (FILE *out, state *s)
{
  size_t i;
  item_number *sitems = s->items;
  size_t snritems = s->nitems;
  symbol *previous_lhs = NULL;

  /* Output all the items of a state, not only its kernel.  */
  if (report_flag & report_itemsets)
    {
      closure (sitems, snritems);
      sitems = itemset;
      snritems = nitemset;
    }

  if (!snritems)
    {
      fprintf(out, " %%empty\n");
      return;
    }

  fputc ('\n', out);

  for (i = 0; i < snritems; i++)
    {
      item_number *sp;
      item_number *sp1;
      rule_number r;

      sp1 = sp = ritem + sitems[i];

      while (*sp >= 0)
        sp++;

      r = item_number_as_rule_number (*sp);

      rule_lhs_print (&rules[r], previous_lhs, out);
      previous_lhs = rules[r].lhs;

      for (sp = rules[r].rhs; sp < sp1; sp++)
        fprintf (out, " %s", symbols[*sp]->tag);
      fputs (" .", out);
      for (/* Nothing */; *sp >= 0; ++sp)
        fprintf (out, " %s", symbols[*sp]->tag);

      /* Display the lookahead tokens?  */
      if (report_flag & report_lookahead_tokens
          && item_number_is_rule_number (*sp1))
        state_rule_lookahead_tokens_print (s, &rules[r], out);

      fputc ('\n', out);
    }
}