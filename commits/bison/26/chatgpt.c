bool
ielr_item_has_lookahead (state *s, symbol_number lhs, size_t item,
                         symbol_number lookahead, state ***predecessors,
                         bitset **item_lookahead_sets)
{
  if (!item_lookahead_sets[s->number])
    {
      item_lookahead_sets[s->number] =
        xnmalloc (s->nitems, sizeof item_lookahead_sets[s->number][0]);
      for (size_t i = 0; i < s->nitems; ++i)
        item_lookahead_sets[s->number][i] = NULL;
    }
  if (!item_lookahead_sets[s->number][item])
    {
      item_lookahead_sets[s->number][item] =
        bitset_create (ntokens, BITSET_FIXED);

      if (s->items[item] > 1)
        {
          if (!lhs)
            lhs = item_rule(&ritem[s->items[item]])->lhs->number;

          size_t item_pos = s->items[item] - 1;
          for (state **predecessor = predecessors[s->number];
               *predecessor;
               ++predecessor)
            {
              for (size_t predecessor_item = 0;
                   predecessor_item < (*predecessor)->nitems;
                   ++predecessor_item)
                {
                  if ((*predecessor)->items[predecessor_item] == item_pos)
                    {
                      ielr_item_has_lookahead(*predecessor, lhs,
                                              predecessor_item, 0,
                                              predecessors, item_lookahead_sets);
                      bitset_or(item_lookahead_sets[s->number][item],
                                item_lookahead_sets[s->number][item],
                                item_lookahead_sets[(*predecessor)->number]
                                                   [predecessor_item]);
                      break;
                    }
                }
            }
        }
    }
  return bitset_test(item_lookahead_sets[s->number][item], lookahead);
}