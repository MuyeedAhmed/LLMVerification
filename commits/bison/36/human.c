/*------------------------------------------------------------------.
| Set TOKEN_TRANSLATIONS.  Check that no two symbols share the same |
| number.                                                           |
`------------------------------------------------------------------*/

static void
symbols_token_translations_init (void)
{
  bool num_256_available_p = true;

  /* Find the highest user token number, and whether 256, the POSIX
     preferred user token number for the error token, is used.  */
  max_user_token_number = 0;
  for (int i = 0; i < ntokens; ++i)
    {
      sym_content *this = symbols[i]->content;
      if (this->user_token_number != USER_NUMBER_UNDEFINED)
        {
          if (this->user_token_number > max_user_token_number)
            max_user_token_number = this->user_token_number;
          if (this->user_token_number == 256)
            num_256_available_p = false;
        }
    }

  /* If 256 is not used, assign it to error, to follow POSIX.  */
  if (num_256_available_p
      && errtoken->content->user_token_number == USER_NUMBER_UNDEFINED)
    errtoken->content->user_token_number = 256;

  /* Set the missing user numbers. */
  if (max_user_token_number < 256)
    max_user_token_number = 256;

  for (int i = 0; i < ntokens; ++i)
    {
      sym_content *this = symbols[i]->content;
      if (this->user_token_number == USER_NUMBER_UNDEFINED)
        {
          IGNORE_TYPE_LIMITS_BEGIN
          if (INT_ADD_WRAPV (max_user_token_number, 1, &max_user_token_number))
            complain (NULL, fatal, _("token number too large"));
          IGNORE_TYPE_LIMITS_END
          this->user_token_number = max_user_token_number;
        }
      if (this->user_token_number > max_user_token_number)
        max_user_token_number = this->user_token_number;
    }

  token_translations = xnmalloc (max_user_token_number + 1,
                                 sizeof *token_translations);

  /* Initialize all entries for literal tokens to the internal token
     number for $undefined, which represents all invalid inputs.  */
  for (int i = 0; i < max_user_token_number + 1; ++i)
    token_translations[i] = undeftoken->content->number;
  for (int i = 0; symbols_sorted[i]; ++i)
    symbol_translation (symbols_sorted[i]);
}