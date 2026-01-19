void
uniqstr_assert (char const *str)
{
  if (!str)
    {
      error (0, 0, "not a uniqstr: NULL");
      abort ();
    }
  char const *existing = hash_lookup (uniqstrs_table, str);
  if (!existing || strcmp (existing, str) != 0)
    {
      error (0, 0, "not a uniqstr: %s", quotearg (str));
      abort ();
    }
}
