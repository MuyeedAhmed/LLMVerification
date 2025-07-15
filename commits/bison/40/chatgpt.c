void
uniqstr_assert (char const *str)
{
  void *entry = NULL;

  for (entry = hash_get_first_entry(uniqstrs_table); 
       entry != NULL; 
       entry = hash_get_next_entry(uniqstrs_table, entry))
  {
    if (entry == str)
      return;
  }

  error (0, 0,
         "not a uniqstr: %s", quotearg (str));
  abort ();
}