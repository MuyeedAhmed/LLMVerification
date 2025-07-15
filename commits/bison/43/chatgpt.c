void
grammar_rules_useless_report (const char *message)
{
  rule_number r;
  for (r = 0; r < nrules ; ++r)
    if (!rules[r].useful && !symbols[rules[r].lhs->number]->useful)
      complain (&rules[r].location, Wother, "%s", message);
}