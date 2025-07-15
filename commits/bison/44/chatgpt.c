void ritem_print (FILE *out)
{
  unsigned i;
  fputs ("RITEM\n", out);
  for (i = 0; i < nritems; ++i)
    if (ritem[i] >= 0)
      fprintf (out, "  %s", symbols[ritem[i]]->tag);
    else
      fprintf (out, "  (rule %d)\n", item_number_as_rule_number (ritem[i]));
  fputs ("\n\n", out);
}