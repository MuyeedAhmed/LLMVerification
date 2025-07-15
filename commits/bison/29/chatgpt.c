void reduce_grammar (void)
{
  /* Allocate the global sets used to compute the reduced grammar */

  N = bitset_create (nnterms, BITSET_FIXED);
  P = bitset_create (nrules, BITSET_FIXED);
  V = bitset_create (nsyms, BITSET_FIXED);
  V1 = bitset_create (nsyms, BITSET_FIXED);

  useless_nonterminals ();
  inaccessable_symbols ();

  /* Did we reduce something? */
  if (nuseless_nonterminals || nuseless_productions)
    {
      reduce_print ();

      if (!bitset_test (N, accept->content->number - ntokens))
        complain (&startsymbol_loc, fatal, _("start symbol %s does not derive any sentence"), startsymbol->tag);

      for (symbol_number i = 0; i < nsyms; ++i)
        {
          symbol *sym = symbols[i];
          if (sym->content->class == nterm && !bitset_test (N, sym->content->number - ntokens))
            complain (&sym->location, Wother, _("start symbol %s does not derive any sentence"), sym->tag);
        }

      /* First reduce the nonterminals, as they renumber themselves in the
         whole grammar. If you change the order, nonterms would be
         renumbered only in the reduced grammar. */
      if (nuseless_nonterminals)
        nonterminals_reduce ();
      if (nuseless_productions)
        reduce_grammar_tables ();
    }

  if (trace_flag & trace_grammar)
    {
      grammar_dump (stderr, "Reduced Grammar");

      fprintf (stderr, "reduced %s defines %d terminals, %d nonterminals, and %d productions.\n",
               grammar_file, ntokens, nnterms, nrules);
    }
}