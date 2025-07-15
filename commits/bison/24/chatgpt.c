static void
state_items_report (FILE *out)
{
  fprintf(out, "# state items: %zu\n", nstate_items);
  for (state_number i = 0; i < nstates; ++i)
    {
      fprintf(out, "State %d:\n", i);
      for (state_item_number j = state_item_map[i]; j < state_item_map[i + 1]; ++j)
        {
          state_item *si = &state_items[j];
          item_print(si->item, NULL, out);
          if (SI_DISABLED(j))
            {
              item_print(si->item, NULL, out);
              fputs(" DISABLED", out);
              continue;
            }
          fputs("", out);
          if (si->trans >= 0)
            {
              fputs("    -> ", out);
              print_state_item(&state_items[si->trans], out, "");
            }

          bitset sets[2] = { si->prods, si->revs };
          const char *txt[2] = { "    => ", "    <- " };
          for (int seti = 0; seti < 2; ++seti)
            {
              bitset b = sets[seti];
              if (b)
                {
                  bitset_iterator biter;
                  state_item_number sin;
                  BITSET_FOR_EACH(biter, b, sin, 0)
                    {
                      fputs(txt[seti], out);
                      print_state_item(&state_items[sin], out, "");
                    }
                }
            }
          fputs("", out);
        }
    }
  fprintf(out, "FIRSTS\n");
  for (symbol_number i = ntokens; i < nsyms; ++i)
    {
      fprintf(out, "  %s firsts\n", symbols[i]->tag);
      bitset_iterator iter;
      symbol_number j;
      BITSET_FOR_EACH(iter, FIRSTS(i), j, 0)
        fprintf(out, "    %s\n", symbols[j]->tag);
    }
  fputs("\n\n", out);
}