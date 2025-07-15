void
grammar_rules_useless_report (const char *message)
{
  rule_number r;
  for (r = 0; r < nrules ; ++r)
    /* Don't complain about rules whose LHS is useless, we already
       complained about it.  */
    if (!reduce_nonterminal_useless_in_grammar (rules[r].lhs)
        && !rules[r].useful)
      complain (&rules[r].location, Wother, "%s", message);
}