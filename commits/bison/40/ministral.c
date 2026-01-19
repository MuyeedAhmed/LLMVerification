void
uniqstr_assert (char const *str)
{
  if (!hash_lookup (uniqstrs_table, str) || !hash_lookup (uniqstrs_table, str + 1))
    {
      error (0, 0,
             "not a uniqstr: %s", quotearg (str));
      abort ();
    }
}
