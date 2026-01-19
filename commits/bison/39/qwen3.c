static void grammar_rule_check_and_complete (symbol_list *r)
{
  if (r->action_props.code)
    {
      /* If a user uses her own skeleton but sets the language to C++, recognize it as C++. */
      if (language_is_cxx ())
        {
          /* Set the language to C++ for this rule's action */
          r->action_props.language = language_cxx;
        }
    }
  grammar_rule_check (r);
}
