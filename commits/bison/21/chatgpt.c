int
main (int argc, char *argv[])
{
#define DEPENDS_ON_LIBINTL 1
#ifdef BISON_PROGRAM_NAME
  argv[0] = BISON_PROGRAM_NAME;
#endif
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  {
    char *cp = NULL;
    char const *localedir = relocate2 (LOCALEDIR, &cp);
    bindtextdomain ("bison", localedir);
    bindtextdomain ("bison-gnulib", localedir);
    bindtextdomain ("bison-runtime", localedir);
    free (cp);
  }
  textdomain ("bison");

  {
    char const *cp = getenv ("LC_CTYPE");
    if (cp && STREQ (cp, "C"))
      set_custom_quoting (&quote_quoting_options, "'", "'");
    else
      set_quoting_style (&quote_quoting_options, locale_quoting_style);
  }

  atexit (close_stdout);

  glyphs_init ();
  uniqstrs_new ();
  muscle_init ();
  complain_init ();

  getargs (argc, argv);

  timevar_enabled = trace_flag & trace_time;
  timevar_init ();
  timevar_start (tv_total);

  if (trace_flag & trace_bitsets)
    bitset_stats_enable ();

  /* Read the input.  Copy some parts of it to FGUARD, FACTION, FTABLE
     and FATTRS.  In file reader.c.  The other parts are recorded in
     the grammar; see gram.h.  */

  timevar_push (tv_reader);
  reader (grammar_file);
  timevar_pop (tv_reader);

  if (complaint_status == status_complaint)
    goto finish;

  /* Find useless nonterminals and productions and reduce the grammar. */
  timevar_push (tv_reduce);
  reduce_grammar ();
  timevar_pop (tv_reduce);

  /* Record other info about the grammar.  In files derives and
     nullable.  */
  timevar_push (tv_sets);
  derives_compute ();
  nullable_compute ();
  timevar_pop (tv_sets);

  /* Compute LR(0) parser states.  See state.h for more info.  */
  timevar_push (tv_lr0);
  generate_states ();
  timevar_pop (tv_lr0);

  /* Add lookahead sets to parser states.  Except when LALR(1) is
     requested, split states to eliminate LR(1)-relative
     inadequacies.  */
  ielr ();

  /* Find and record any conflicts: places where one token of
     lookahead is not enough to disambiguate the parsing.  In file
     conflicts.  Also resolve s/r conflicts based on precedence
     declarations.  */
  timevar_push (tv_conflicts);
  conflicts_solve ();
  if (!muscle_percent_define_flag_if ("lr.keep-unreachable-state"))
    {
      state_number *old_to_new = xnmalloc (nstates, sizeof *old_to_new);
      state_number nstates_old = nstates;
      state_remove_unreachable_states (old_to_new);
      lalr_update_state_numbers (old_to_new, nstates_old);
      conflicts_update_state_numbers (old_to_new, nstates_old);
      free (old_to_new);
    }
  if (report_flag & report_cex
      || warning_is_enabled (Wcounterexamples))
    counterexample_init ();
  conflicts_print ();
  timevar_pop (tv_conflicts);

  /* Compute the parser tables.  */
  timevar_push (tv_actions);
  tables_generate ();
  timevar_pop (tv_actions);

  grammar_rules_useless_report (_("rule useless in parser due to conflicts"));

  print_precedence_warnings ();

  /* Whether to generate output files.  */
  bool generate = !(feature_flag & feature_syntax_only);

  if (generate)
    {
      /* Output file names. */
      compute_output_file_names ();

      /* Output the detailed report on the grammar.  */
      if (report_flag)
        {
          timevar_push (tv_report);
          print_results ();
          timevar_pop (tv_report);
        }

      /* Output the graph.  */
      if (graph_flag)
        {
          timevar_push (tv_graph);
          print_graph ();
          timevar_pop (tv_graph);
        }

      /* Output xml.  */
      if (xml_flag)
        {
          timevar_push (tv_xml);
          print_xml ();
          timevar_pop (tv_xml);
        }
    }

  /* Stop if there were errors, to avoid trashing previous output
     files.  */
  if (complaint_status == status_complaint)
    goto finish;

  /* Lookahead tokens are no longer needed. */
  timevar_push (tv_free);
  lalr_free ();
  timevar_pop (tv_free);

  /* Output the tables and the parser to ftable.  In file output.  */
  if (generate)
    {
      timevar_push (tv_parser);
      output ();
      timevar_pop (tv_parser);
    }

 finish:

  timevar_push (tv_free);
  nullable_free ();
  derives_free ();
  tables_free ();
  states_free ();
  reduce_free ();
  conflicts_free ();
  grammar_free ();
  counterexample_free ();
  output_file_names_free ();

  /* The scanner and parser memory cannot be released right after
     parsing, as it contains things such as user actions, prologue,
     epilogue etc.  */
  gram_scanner_free ();
  parser_free ();

  muscle_free ();
  code_scanner_free ();
  skel_scanner_free ();
  timevar_pop (tv_free);

  if (trace_flag & trace_bitsets)
    bitset_stats_dump (stderr);

  /* Stop timing and print the times.  */
  timevar_stop (tv_total);
  timevar_print (stderr);

  /* Fix input file now, even if there are errors: that's less
     warnings in the following runs.  */
  if (!fixits_empty ())
    {
      if (update_flag)
        fixits_run ();
      else
        complain (NULL, Wother,
                  _("fix-its can be applied.  Rerun with option '--update'."));
      fixits_free ();
    }
  uniqstrs_free ();

  complain_free ();
  quotearg_free ();

  return complaint_status ? EXIT_FAILURE : EXIT_SUCCESS;
}