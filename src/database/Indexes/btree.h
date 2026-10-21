#pragma once

#include "btree_node.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

class BTree {
private:
    std::unique_ptr<BTreeNode> root;
    int minimum_degree;
    std::size_t row_count = 0;

public:
    explicit BTree(int minimum_degree = 3);

    bool empty() const;

    std::size_t size() const;

    void insert(const Value& key, RowID row_id);

    std::optional<std::vector<RowID>> search(
        const Value& key
    ) const;

private:
    std::optional<std::vector<RowID>> search_node(
        const BTreeNode* node,
        const Value& key
    ) const;

    void insert_non_full(
        BTreeNode* node,
        const IndexEntry& entry
    );

    void split_child(
        BTreeNode* parent,
        std::size_t child_index
    );
};
