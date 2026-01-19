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
            lhs = item_rule (&ritem[s->items[item]])->lhs->number;
          if (item_number_is_rule_number (ritem[s->items[item] - 2]))
            {
              aver (lhs != accept->content->number);
              for (state **predecessor = predecessors[s->number];
                   *predecessor;
                   ++predecessor)
                bitset_or (item_lookahead_sets[s->number][item],
                           item_lookahead_sets[s->number][item],
                           goto_follows[map_goto ((*predecessor)->number,
                                                  lhs)]);
            }
          else
            {
              for (state **predecessor = predecessors[s->number];
                   *predecessor;
                   ++predecessor)
                {
                  size_t predecessor_item;
                  for (predecessor_item = 0;
                       predecessor_item < (*predecessor)->nitems;
                       ++predecessor_item)
                    if ((*predecessor)->items[predecessor_item]
                        == s->items[item] - 1)
                      break;
                  aver (predecessor_item != (*predecessor)->nitems);
                  ielr_item_has_lookahead (*predecessor, lhs,
                                           predecessor_item, 0 /*irrelevant*/,
                                           predecessors, item_lookahead_sets);
                  bitset_or (item_lookahead_sets[s->number][item],
                             item_lookahead_sets[s->number][item],
                             item_lookahead_sets[(*predecessor)->number]
                                                [predecessor_item]);
                }
            }
        }
    }
  return bitset_test (item_lookahead_sets[s->number][item], lookahead);
}
