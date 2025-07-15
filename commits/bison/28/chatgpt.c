static void
grammar_rule_check_and_complete (symbol_list *r)
{
  /* Type check. */
  if (!r->action_props.code && r->content.sym->content->type_name)
    {
      symbol *first_rhs = r->next->content.sym;
      if (first_rhs)
        {
          char const *lhs_type = r->content.sym->content->type_name;
          char const *rhs_type =
            first_rhs->content->type_name ? first_rhs->content->type_name : "";
          if (!UNIQSTR_EQ (lhs_type, rhs_type))
            complain (&r->rhs_loc, Wother,
                      _("type clash on default action: <%s> != <%s>"),
                      lhs_type, rhs_type);
          else
            {
              const bool is_cxx =
                STREQ (language->language, "c++")
                || (skeleton && (STREQ (skeleton, "glr.cc")
                                 || STREQ (skeleton, "lalr1.cc")));
              if (is_cxx)
                {
                  code_props_rule_action_init (&r->action_props, "{ $$ = $1; }",
                                               r->rhs_loc, r,
                                               /* name */ NULL,
                                               /* type */ NULL,
                                               /* is_predicate */ false);
                  code_props_translate_code (&r->action_props);
                }
            }
        }
      else
        complain (&r->rhs_loc, Wother,
                  _("empty rule for typed nonterminal, and no action"));
    }

  /* Check symbol values. */
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
              complain (&l->sym_loc, warn_flag, _("unused value: $%d"), n);
            else
              complain (&l->rhs_loc, warn_flag, _("unset value: $$"));
          }
      }
  }

  /* Check %empty. */
  if (r->percent_empty_loc.start.file
      && r->next && r->next->content.sym)
    {
      complain (&r->percent_empty_loc, complaint,
                _("%%empty on non-empty rule"));
      fixits_register (&r->percent_empty_loc, "");
    }

  if (!(r->next && r->next->content.sym)
      && !r->midrule_parent_rule
      && !r->percent_empty_loc.start.file
      && warning_is_enabled (Wempty_rule))
    {
      complain (&r->rhs_loc, Wempty_rule, _("empty rule without %%empty"));
      if (feature_flag & feature_caret)
        location_caret_suggestion (r->rhs_loc, "%empty", stderr);
      location loc = r->rhs_loc;
      loc.end = loc.start;
      fixits_register (&loc, " %empty ");
    }

  if (r->ruleprec
      && r->ruleprec->tag[0] != '\'' && r->ruleprec->tag[0] != '"'
      && r->ruleprec->content->status != declared
      && !r->ruleprec->content->prec)
    complain (&r->rhs_loc, Wother,
              _("token for %%prec is not defined: %s"), r->ruleprec->tag);

  if (r->action_props.type)
    complain (&r->rhs_loc, Wother,
              _("only midrule actions can be typed: %s"), r->action_props.type);

  /* Generate the actions for each start symbol. */
  for (symbol_list *p = grammar; p; p = p->next)
    {
      if (symbol_is_nonterm (p->content.sym))
        {
          code_props_rule_action_init (&p->action_props, "{ $$ = $1; }",
                                       p->rhs_loc, p,
                                       /* name */ NULL,
                                       /* type */ NULL,
                                       /* is_predicate */ false);
          code_props_translate_code (&p->action_props);
        }
    }
}