void
grammar_dump (FILE *out, const char *title)
{
  fprintf (out, "%s\n\n", title);
  fprintf (out,
           "ntokens = %d, nvars = %d, nsyms = %d, nrules = %d, nritems = %d\n\n",
           ntokens, nvars, nsyms, nrules, nritems);

  fprintf (out, "Tokens\n------\n\n");
  {
    fprintf (out, "Value  Sprec  Sassoc  Tag\n");

    for (symbol_number i = 0; i < ntokens; i++)
      fprintf (out, "%5d  %5d   %5d  %s\n",
               i,
               symbols[i]->content->prec, symbols[i]->content->assoc,
               symbols[i]->tag);
    fprintf (out, "\n\n");
  }

  fprintf (out, "Non terminals\n-------------\n\n");
  {
    fprintf (out, "Value  Tag\n");

    for (symbol_number i = ntokens; i < nsyms; i++)
      fprintf (out, "%5d  %s\n",
               i, symbols[i]->tag);
    fprintf (out, "\n\n");
  }

  fprintf (out, "Rules\n-----\n\n");
  {
    fprintf (out,
             "Num (Prec, Assoc, Useful, UselessChain) Lhs"
             " -> (Ritem Range) Rhs\n");
    for (rule_number i = 0; i < nrules + nuseless_productions; ++i)
      {
        rule const *rule_i = &rules[i];
        int const rhs_itemno = rule_i->rhs - ritem;
        int length = rule_rhs_length (rule_i);
        aver (item_number_as_rule_number (rule_i->rhs[length]) == i);
        fprintf (out, "%3d (%2d, %2d, %2s, %2s)   %2d -> (%2u-%2u)",
                 i,
                 rule_i->prec ? rule_i->prec->prec : 0,
                 rule_i->prec ? rule_i->prec->assoc : 0,
                 rule_i->useful ? "t" : "f",
                 rule_useless_chain_p (rule_i) ? "t" : "f",
                 rule_i->lhs->number,
                 rhs_itemno, rhs_itemno + length - 1);
        /* Dumped the RHS. */
        for (item_number *rhsp = rule_i->rhs; 0 <= *rhsp; ++rhsp)
          fprintf (out, " %3d", *rhsp);
        fputc ('\n', out);
      }
  }
  fprintf (out, "\n\n");

  fprintf (out, "Rules interpreted\n-----------------\n\n");
  for (rule_number r = 0; r < nrules + nuseless_productions; ++r)
    {
      fprintf (out, "%-5d  %s:", r, rules[r].lhs->symbol->tag);
      rule_rhs_print (&rules[r], out);
      fputc ('\n', out);
    }
  fprintf (out, "\n\n");
}