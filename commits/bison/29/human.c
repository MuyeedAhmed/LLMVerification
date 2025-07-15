void
reduce_grammar (void)
{
  /* Allocate the global sets used to compute the reduced grammar */

  N = bitset_create (nnterms, BITSET_FIXED);
  P =  bitset_create (nrules, BITSET_FIXED);
  V = bitset_create (nsyms, BITSET_FIXED);
  V1 = bitset_create (nsyms, BITSET_FIXED);

  useless_nonterminals ();
  inaccessable_symbols ();

  /* Did we reduce something? */
  if (nuseless_nonterminals || nuseless_productions)
    {
      reduce_print ();

      // Check that start symbols have non-empty languages.
      bool failure = false;
      for (symbol_list *list = start_symbols; list; list = list->next)
        if (!bitset_test (N, list->content.sym->content->number - ntokens))
          {
            failure = true;
            complain (&list->sym_loc, complaint,
                      _("start symbol %s does not derive any sentence"),
                      list->content.sym->tag);
          }
      if (failure)
        exit (EXIT_FAILURE);

      /* First reduce the nonterminals, as they renumber themselves in the
         whole grammar.  If you change the order, nonterms would be
         renumbered only in the reduced grammar.  */
      if (nuseless_nonterminals)
        nonterminals_reduce ();
      if (nuseless_productions)
        reduce_grammar_tables ();
    }

  if (trace_flag & trace_grammar)
    {
      grammar_dump (stderr, "Reduced Grammar");

      fprintf (stderr, "reduced %s defines %d terminals, %d nonterminals"
               ", and %d productions.\n",
               grammar_file, ntokens, nnterms, nrules);
    }
}