void
ritem_print (FILE *out)
{
  fputs ("RITEM\n", out);
  bool first = true;
  for (int i = 0; i < nritems; ++i)
    {
      if (first)
        {
          fprintf (out, "  %d: ", i);
          first = false;
        }
      if (ritem[i] >= 0)
        fprintf (out, "  %s", symbols[ritem[i]]->tag);
      else
        {
          fprintf (out, "  (rule %d)\n", item_number_as_rule_number (ritem[i]));
          first = true;
        }
    }
  fputs ("\n\n", out);
}