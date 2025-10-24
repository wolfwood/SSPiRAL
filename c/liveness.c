#include "liveness.h"

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


// a faster, non-recursive version of checkIfAlive

// XXX: depth should ever be more than N, see NOTES
bool deadnessCheck(const node_t *Is, const int len) {
  int j = len;

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

      // one greater than the highest acceptable value - gets fed into the next element
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
