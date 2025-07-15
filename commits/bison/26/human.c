/**
 * \note
 *   - FIXME: It might be an interesting experiment to compare the space and
 *     time efficiency of computing \c item_lookahead_sets either:
 *     - Fully up front.
 *     - Just-in-time, as implemented below.
 *     - Not at all.  That is, just let annotations continue even when
 *       unnecessary.
 */
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
      /* If this kernel item is the beginning of a RHS, it must be the kernel
         item in the start state, and so its LHS has no follows and no goto to
         check.  If, instead, this kernel item is the successor of the start
         state's kernel item, there are still no follows and no goto.  This
         situation is fortunate because we want to avoid the - 2 below in both
         cases.

         Actually, IELR(1) should never invoke this function for either of
         those cases because (1) follow_kernel_items will never reference a
         kernel item for this RHS because the end token blocks sight of the
         lookahead set from the RHS's only nonterminal, and (2) no reduction
         has a lookback dependency on this lookahead set.  Nevertheless, I
         didn't change this test to an aver just in case the usage of this
         function evolves to need those two cases.  In both cases, the current
         implementation returns the right result.  */
      const rule *r = item_rule (&ritem[s->items[item]]);
      const bool is_successor_of_initial_item
        = rule_is_initial (r) && &ritem[s->items[item]] == r->rhs + 1;
      aver (!is_successor_of_initial_item);
      if (!is_successor_of_initial_item)
        {
          /* If the LHS symbol of this item isn't known (because this is a
             top-level invocation), go get it.  */
          if (!lhs)
            lhs = r->lhs->number;
          /* If this kernel item is next to the beginning of the RHS, then
             check all predecessors' goto follows for the LHS.  */
          if (item_number_is_rule_number (ritem[s->items[item] - 2]))
            {
              aver (lhs != acceptsymbol->content->number);
              for (state **predecessor = predecessors[s->number];
                   *predecessor;
                   ++predecessor)
                bitset_or (item_lookahead_sets[s->number][item],
                           item_lookahead_sets[s->number][item],
                           goto_follows[map_goto ((*predecessor)->number,
                                                  lhs)]);
            }
          /* If this kernel item is later in the RHS, then check all
             predecessor items' lookahead sets.  */
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