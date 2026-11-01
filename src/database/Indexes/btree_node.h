#pragma once

#include "index_entry.h"

#include <memory>
#include <memory_resource>

struct BTreeNode;

struct BTreeNodeDeleter {
    std::pmr::memory_resource *resource = std::pmr::get_default_resource();
    void operator()(BTreeNode *node) const noexcept;
};

using BTreeNodePtr = std::unique_ptr<BTreeNode, BTreeNodeDeleter>;

struct BTreeNode {
    bool is_leaf = true;

    std::pmr::vector<IndexEntry> entries;

    std::pmr::vector<BTreeNodePtr> children;

    explicit BTreeNode(std::pmr::memory_resource *resource)
        : entries(resource), children(resource) {}
};

inline void BTreeNodeDeleter::operator()(BTreeNode *node) const noexcept {
    if (!node)
        return;
    std::destroy_at(node);
    std::pmr::polymorphic_allocator<BTreeNode>{resource}.deallocate(node, 1);
}
