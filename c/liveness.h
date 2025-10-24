// functions for evaluating whether a layout has experienced data loss

#pragma once

#include "types.h"

bool checkIfAlive(layout_t name);
bool deadnessCheck(const node_t *Is, const int len);
