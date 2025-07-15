/*-----------------------------------------------------------------.
| Check that the rule R is properly defined.  For instance, there  |
| should be no type clash on the default action.  Possibly install |
| the default action.                                              |
`-----------------------------------------------------------------*/

static void
grammar_rule_check_and_complete (symbol_list *r)
{
  /* Type check.

     If there is an action, then there is nothing we can do: the user
     is allowed to shoot herself in the foot.

     Don't worry about the default action if $$ is untyped, since $$'s
     value can't be used.  */
  if (!r->action_props.code && r->content.sym->content->type_name)
    {
      symbol *first_rhs = r->next->content.sym;
      /* If $$ is being set in default way, report if any type mismatch.  */
      if (first_rhs)
        {
          char const *lhs_type = r->content.sym->content->type_name;
          const char *rhs_type =
            first_rhs->content->type_name ? first_rhs->content->type_name : "";
          if (!UNIQSTR_EQ (lhs_type, rhs_type))
            complain (&r->location, Wother,
                      _("type clash on default action: <%s> != <%s>"),
                      lhs_type, rhs_type);
          else
            {
              /* Install the default action only for C++.  */
              const bool is_cxx =
                STREQ (language->language, "c++")
                || (skeleton && (STREQ (skeleton, "glr.cc")
                                 || STREQ (skeleton, "lalr1.cc")));
              if (is_cxx)
                {
                  code_props_rule_action_init (&r->action_props, "{ $$ = $1; }",
                                               r->location, r,
                                               /* name */ NULL,
                                               /* type */ NULL,
                                               /* is_predicate */ false);
                  code_props_translate_code (&r->action_props);
                }
            }
        }
      /* Warn if there is no default for $$ but we need one.  */
      else
        complain (&r->location, Wother,
                  _("empty rule for typed nonterminal, and no action"));
    }

  /* Check that symbol values that should be used are in fact used.  */
  {
    int n = 0;
    for (symbol_list const *l = r; l && l->content.sym; l = l->next, ++n)
      {
        bool midrule_warning = false;
        if (!l->action_props.is_value_used
            && symbol_should_be_used (l, &midrule_warning)
            /* The default action, $$ = $1, 'uses' both.  */
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

  /* Check that %empty => empty rule.  */
  if (r->percent_empty_loc.start.file
      && r->next && r->next->content.sym)
    complain (&r->percent_empty_loc, complaint,
              _("%%empty on non-empty rule"));

  /* Check that empty rule => %empty.  */
  if (!(r->next && r->next->content.sym)
      && !r->midrule_parent_rule
      && !r->percent_empty_loc.start.file)
    complain (&r->location, Wempty_rule, _("empty rule without %%empty"));

  /* See comments in grammar_current_rule_prec_set for how POSIX
     mandates this complaint.  It's only for identifiers, so skip
     it for char literals and strings, which are always tokens.  */
  if (r->ruleprec
      && r->ruleprec->tag[0] != '\'' && r->ruleprec->tag[0] != '"'
      && r->ruleprec->content->status != declared
      && !r->ruleprec->content->prec)
    complain (&r->location, Wother,
              _("token for %%prec is not defined: %s"), r->ruleprec->tag);

  /* Check that the (main) action was not typed.  */
  if (r->action_props.type)
    complain (&r->location, Wother,
              _("only midrule actions can be typed: %s"), r->action_props.type);
}