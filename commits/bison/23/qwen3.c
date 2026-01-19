static void start_symbols_output (FILE *out)
{
  if (nsyms > ntokens)
    {
      fprintf (out, "m4_define([b4_start_symbols],\n[[");
      for (int i = ntokens; i < nsyms; ++i)
        {
          if (i > ntokens)
            fprintf (out, "\n  ");
          fprintf (out, "[%d, %d]", symbols[i]->content->number, symbols[i]->content->code);
        }
      fprintf (out, "]]\n\n");
    }
}
