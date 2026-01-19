void
ritem_print (FILE *out)
{
  fputs ("RITEM\n", out);
  for (int i = 0; i < nritems; ++i)
    {
      fprintf (out, "  %3d: ", i);
      if (ritem[i] >= 0)
        fprintf (out, "%s", symbols[ritem[i]]->tag);
      else
        fprintf (out, "(rule %d)", item_number_as_rule_number (ritem[i]));
      fputs ("\n", out);
    }
  fputs ("\n", out);
}
