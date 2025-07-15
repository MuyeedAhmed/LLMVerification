lssi_print (lssi *l)
{
  FILE *out = stderr;
  print_state_item (&state_items[l->si], out);
  if (l->lookahead)
    {
      fprintf (out, "FOLLOWL = { ");
      bitset_iterator biter;
      symbol_number sin;
      BITSET_FOR_EACH (biter, l->lookahead, sin, 0)
        fprintf (out, "%s, \n", symbols[sin]->tag);
      fprintf (out, "}\n");
    }
}