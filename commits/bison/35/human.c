/*------------------------------------------------.
| Set the USER_TOKEN_NUMBER associated with SYM.  |
`------------------------------------------------*/

void
symbol_user_token_number_set (symbol *sym, int user_token_number, location loc)
{
  int *user_token_numberp = &sym->content->user_token_number;
  if (sym->content->class != token_sym)
    complain (&loc, complaint,
              _("nonterminals cannot be given an explicit number"));
  else if (*user_token_numberp != USER_NUMBER_UNDEFINED
           && *user_token_numberp != user_token_number)
    complain (&loc, complaint, _("redefining user token number of %s"),
              sym->tag);
  else if (user_token_number == INT_MAX)
    complain (&loc, complaint, _("user token number of %s too large"),
              sym->tag);
  else
    {
      *user_token_numberp = user_token_number;
      /* User defined $end token? */
      if (user_token_number == 0 && !endtoken)
        {
          endtoken = sym->content->symbol;
          /* It is always mapped to 0, so it was already counted in
             NTOKENS.  */
          if (endtoken->content->number != NUMBER_UNDEFINED)
            --ntokens;
          endtoken->content->number = 0;
        }
    }
}