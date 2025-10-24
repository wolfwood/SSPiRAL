#include "liveness.h"

/* Liveness is a metaphor from the game of Go. Even if a layout does not contain
   every data node, as long as every missing data node can be reconstructed from
   parities the layout is alive.
 */


/* In the basic implementation, the layout to be considered is encoded as a
   bitmap, name. The outer loop, in checkIfAlive(), enumerates the data nodes
   and for every one that is missing initiates an attempt to reconstruct it,
   using recurseCheck().

   Rather than passing the node to be constructed to recurseCheck() as an explicit
   value to compare against, the canceling-out effect of xor is used, and
   instead the node is passed in as n. The recursion tests each remaining node
   that can be found in the bitmap, first xor-ing it with n and recursing, then
   omitting it and testing the next value. The recursion completes when xor-ing
   a node with n produces zero; the fact that it has canceled out indicates that
   there exists a subset of nodes that, when xor-ed together, produce the
   initial value that was passed in.

   An exhausted recursive search means that the given data node cannot be
   reconstructed, and so the layout is dead, regardless of the results for the
   remaining nodes.
 */

static bool recurseCheck(const layout_t name, node_t n, node_t i) {
  for (; i <= M; ++i) {
    layout_t l = ((layout_t) 1) << (i - 1);

    if (l & name) {
      layout_t temp = i ^n;

      if (temp == 0 || recurseCheck(name, temp, i + 1)) {
        return true;
      }
    }
  }

  return false;
}

bool checkIfAlive(const layout_t name) {
  for (node_t n = 1; n <= M; n <<= 1) {
    layout_t l = node2layout(n);

    if ((l & name) == 0) {
      if (!recurseCheck(name, n, 1)) {
        return false;
      }
    }
  }
  return true;
}


/* A faster, non-recursive version of checkIfAlive() that operates on a
   different layout representation, and inverts the return value. It is more
   useful to track deadness than liveness as the value is smaller and
   potentially fits into a smaller type.

   The purpose of this is to save the time previously spent encoding the layout
   'name' bitmap in the iterator and then decoding it in the liveness check.
   Instead the internal state of the iteration is used directly.

   The Is[] array contains len (at least N, at most M) + 1 elements, with the
   0th serving as a sentinel value. The values, excluding the sentinel, are in
   descending order. The elements are the i's of an iteration over all possible
   sets of nodes with len elements. i's start counting at zero, so we add 1 to
   get the corresponding node name. This is in part because the loops are use
   zero as a sentinel, and in part because when the code did construct bitmaps,
   converting from node to layout is normally done by ((layout_t) 1) << (n - 1),
   so this range shift eliminated the need for the subtraction.

   the outer loop still enumerates nodes but now it scans backwards through the
   Is[] using j as cursor. If the given data node is absent, a Js[] array is
   created for tracking the state of each node in the reconstruction process.

   Each node can be in one of 3 states. Initially all nodes are set to SENTINEL,
   which indicates the node ready to be included in a reconstruction. After
   including the node, and checking for a successful reconstruction, the state
   moves to 1 and iteration continues with the next element unless we are at the
   end of the array. When a 1 is encountered, the node is xor-ed again, backing
   it out from the reconstruction as none of the search path including this node
   and the current state of it predecessors was successful. The state is set to
   0 and iteration continues. Note that the success check happens again
   needlessly, but any effort to avoid this would also involve a branch, so the
   effort would be the same. When a 0 state is encountered, we reset the state
   to the sentinel and move backwards in the array. If we ever move into the 0th
   element, the inner loop terminates and we know that the search has been
   unsuccessful.
 */

// XXX: should never need more than N nodes for reconstruction, see Invariants
// XXX: also, test iterated Depth-First Search instead
bool deadnessCheck(const node_t *Is, const int len) {
  int j = len;

  // counting down isn't faster
  //  for(node_t n = 1 << (N-1); n > 0; n >>= 1) {
  for (node_t n = 1; n < M; n <<= 1) {
    while (j > 1 && (Is[j] + 1) < n) {
      --j;
    }

    if ((Is[j] + 1) != n) {
      node_t temp = n;

      node_t Js[len + 1];
      const node_t SENTINEL = 2;

      for (uint k = 1; k <= len; ++k) {
        Js[k] = SENTINEL;
      }

      Js[0] = 0;

      bool alive = false;
      uint i = 1;

      while (0 < i) {
        if (0 == Js[i]) {
          //temp ^= Is[i];
          Js[i] = SENTINEL;
          --i;
        } else {
          temp ^= Is[i] + 1;

          if (0 == temp) {
            // continue outer loop
            alive = true;
            break;
          }

          --Js[i];
          if (len > i) {
            ++i;
          }
        }
      }

      if (!alive) {
        return true;
      }
    }
  }

  return false;
}
