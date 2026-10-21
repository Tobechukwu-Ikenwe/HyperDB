#pragma once

#include "value.h"

#include <vector>

// A row stores one value for each column in schema order.
using Row = std::vector<Value>;
