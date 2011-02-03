#include "btree/rget.hpp"
#include "btree/iteration.hpp"
#include "containers/iterators.hpp"
#include "arch/linux/coroutines.hpp"

/*
 * Possible rget designs:
 * 1. Depth-first search through the B-tree, then iterating through leaves (and maintaining a stack
 *    with some data to be able to backtrack).
 * 2. Breadth-first search, by maintaining a queue of blocks and releasing the lock on the block
 *    when we extracted the IDs of its children.
 * 3. Hybrid of 1 and 2: maintain a deque and use it as a queue, like in 2, thus releasing the locks
 *    for the top of the B-tree quickly, however when the deque reaches some size, start using it as
 *    a stack in depth-first search (but not quite in a usual way; see the note below).
 *
 * Problems of 1: we have to lock the whole path from the root down to the current node, which works
 * fine with small rgets (when max_results is low), but causes unnecessary amounts of locking (and
 * probably copy-on-writes, once we implement them).
 *
 * Problem of 2: while it doesn't hold unnecessary locks to the top (close to root) levels of the
 * B-tree, it may try to lock too much at once if the rget query effectively spans too many blocks
 * (e.g. when we try to rget the whole database).
 *
 * Hybrid approach seems to be the best choice here, because we hold the locks as low (far from the
 * root) in the tree as possible, while minimizing their number by doing a depth-first search from
 * some level.
 *
 * Note (on hybrid implementation):
 * If the deque approach is used, it is important to note that all the nodes in the current level
 * are in a reversed order when we decide to switch to popping from the stack:
 *
 *      P       Lets assume that we have node P in our deque, P is locked: [P]
 *    /   \     We remove P from the deque, lock its children, and push them back to the deque: [A, B]
 *   A     B    Now we can release the P lock.
 *  /|\   /.\   Next, we remove A, lock its children, and push them back to the deque: [B, c, d, e]
 * c d e .....  We release the A lock.
 * ..... ...... 
 *              At this point we decide that we need to do a depth-first search (to limit the number
 * of locked nodes), and start to use deque as a stack. However since we want
 * an inorder traversal, not the reversed inorder, we can't pop from the end of
 * the deque, we need to pop node 'c' instead of 'e', then (once we're done
 * with its depth-first search) do 'd', and then do 'e'.
 *
 * There are several possible approaches, one of them is putting markers in the deque in
 * between the nodes of different B-tree levels, another (probably a better one) is maintaining a
 * deque of deques, where the inner deques contain the nodes from the current B-tree level.
 *
 *
 * Currently the DFS design is implemented, since it's the simplest solution, also it is a good
 * fit for small rgets (the most popular use-case).
 *
 *
 * Most of the implementation now resides in btree/iteration.{hpp,cc}.
 */

store_t::rget_result_t btree_rget(btree_key_value_store_t *store, store_key_t *start, store_key_t *end, bool left_open, bool right_open, uint64_t max_results) {
    store_t::rget_result_t result;
    std::vector<boost::shared_ptr<transactor_t> > transactors;

    thread_saver_t thread_saver;

    merge_ordered_data_iterator_t<key_with_data_provider_t,key_with_data_provider_t::less>::mergees_t ms;
    for (int s = 0; s < store->btree_static_config.n_slices; s++) {
        btree_slice_t *slice = store->slices[s];
        boost::shared_ptr<transactor_t> transactor = boost::shared_ptr<transactor_t>(new transactor_t(&slice->cache, rwi_read));
        transactors.push_back(transactor);
        ms.push_back(new slice_keys_iterator_t(transactor, slice, start, end, left_open, right_open));
    }

    merge_ordered_data_iterator_t<key_with_data_provider_t,key_with_data_provider_t::less> merge_iterator(ms);
    boost::optional<key_with_data_provider_t> pair;
    while (pair = merge_iterator.next()) {
        result.results.push_back(pair.get());

        if (result.results.size() == max_results)
            break;
    }

    while (!ms.empty()) {
        delete ms.back();
        ms.pop_back();
    }
    return result;
}

store_t::rget_result_t btree_rget_slice_iterator(btree_slice_t *slice, store_key_t *start, store_key_t *end, bool left_open, bool right_open, uint64_t max_results) {
    return store_t::rget_result_t();
}

store_t::rget_result_t btree_rget_slice(btree_slice_t *slice, store_key_t *start, store_key_t *end, bool left_open, bool right_open, uint64_t max_results) {
    return store_t::rget_result_t();
}

rget_large_value_provider_t::~rget_large_value_provider_t() {
    if (large_value) {
        large_value->release();
    }
}

rget_value_provider_t *rget_value_provider_t::create_provider(const btree_value *value, const boost::shared_ptr<transactor_t>& transactor) {
    if (value->is_large())
        return new rget_large_value_provider_t(value, transactor);
    else
        return new rget_small_value_provider_t(value);
}

