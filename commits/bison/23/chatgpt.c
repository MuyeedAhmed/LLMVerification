static void
start_symbols_output (FILE *out)
{
  fputs ("m4_define([b4_starts],\n[", out);
  bool first = true;
  for (start_symbol_list *p = start_symbols; p; p = p->next)
    {
      if (first)
        first = false;
      else
        fputs (", ", out);

      fprintf (out, "[%d, %d]", p->start->number, p->symbol);
    }
  fputs ("])\n\n", out);
}