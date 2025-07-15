void
ritem_print (FILE *out)
{
  fputs ("RITEM\n", out);
  for (int i = 0; i < nritems; ++i)
    if (ritem[i] >= 0)
      fprintf (out, "  [%d] %s\n", i, symbols[ritem[i]]->tag);
    else
      fprintf (out, "  [%d] (rule %d)\n", i, item_number_as_rule_number (ritem[i]));
  fputs ("\n\n", out);
}