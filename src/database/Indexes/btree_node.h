#pragma once

#include "index_entry.h"

#include <memory>
#include <vector>

struct BTreeNode {
    bool is_leaf = true;

    std::vector<IndexEntry> entries;

    std::vector<std::unique_ptr<BTreeNode>> children;
};
