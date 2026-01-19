static void
grammar_rule_check_and_complete (symbol_list *r)
{
  /* Check if the language is C++ */
  if (muscle_percent_define_get ("language") && strstr(muscle_percent_define_get("language"), "C++"))
    {
      /* Mark the rule as C++ specific */
      r->content.sym->content->is_cpp = true;
    }

  /* Type check. */
  if (!r->action_props.code && r->content.sym->content->type_name)
    {
      symbol *first_rhs = r->next->content.sym;
      if (first_rhs)
        {
          char const *lhs_type = r->content.sym->content->type_name;
          const char *rhs_type = first_rhs->content->type_name ? first_rhs->content->type_name : "";
          if (!UNIQSTR_EQ (lhs_type, rhs_type))
            complain (&r->location, Wother,
                      _("type clash on default action: <%s> != <%s>"),
                      lhs_type, rhs_type);
        }
      else
        complain (&r->location, Wother,
                  _("empty rule for typed nonterminal, and no action"));
    }

  /* Check that symbol values that should be used are in fact used. */
  {
    int n = 0;
    for (symbol_list const *l = r; l && l->content.sym; l = l->next, ++n)
      {
        bool midrule_warning = false;
        if (!l->action_props.is_value_used
            && symbol_should_be_used (l, &midrule_warning)
            && (r->action_props.code || (n != 0 && n != 1)))
          {
            warnings warn_flag = midrule_warning ? Wmidrule_values : Wother;
            if (n)
              complain (&l->location, warn_flag, _("unused value: $%d"), n);
            else
              complain (&l->location, warn_flag, _("unset value: $$"));
          }
      }
  }

  /* Check that %empty => empty rule. */
  if (r->percent_empty_loc.start.file
      && r->next && r->next->content.sym)
    complain (&r->percent_empty_loc, complaint,
              _("%%empty on non-empty rule"));

  /* Check that empty rule => %empty. */
  if (!(r->next && r->next->content.sym)
      && !r->midrule_parent_rule
      && !r->percent_empty_loc.start.file)
    complain (&r->location, Wempty_rule, _("empty rule without %%empty"));

  /* See comments in grammar_current_rule_prec_set for how POSIX
     mandates this complaint. */
  if (r->ruleprec
      && r->ruleprec->tag[0] != '\'' && r->ruleprec->tag[0] != '"'
      && r->ruleprec->content->status != declared
      && !r->ruleprec->content->prec)
    complain (&r->location, Wother,
              _("token for %%prec is not defined: %s"), r->ruleprec->tag);

  /* Check that the (main) action was not typed. */
  if (r->action_props.type)
    complain (&r->location, Wother,
              _("only midrule actions can be typed: %s"), r->action_props.type);
}
