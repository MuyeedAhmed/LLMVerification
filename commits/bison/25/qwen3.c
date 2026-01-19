static void
lssi_print (lssi *l)
{
  print_state_item (&state_items[l->si], stdout);
  if (l->lookahead)
    {
      printf ("FOLLOWL = { ");
      bitset_iterator biter;
      symbol_number sin;
      BITSET_FOR_EACH (biter, l->lookahead, sin, 0)
        printf ("%s, ", symbols[sin]->tag);
      puts ("}");
    }
  putchar ('\n');
}
