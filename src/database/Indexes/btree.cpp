#include "btree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {
bool numeric(const Value& value) {
    return std::holds_alternative<std::int64_t>(value) || std::holds_alternative<double>(value);
}

long double number(const Value& value) {
    return std::holds_alternative<double>(value)
        ? static_cast<long double>(std::get<double>(value))
        : static_cast<long double>(std::get<std::int64_t>(value));
}

int rank(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return 0;
    if (numeric(value)) return 1;
    if (std::holds_alternative<std::string>(value)) return 2;
    return 3;
}

bool less(const Value& a, const Value& b) {
    if (rank(a) != rank(b)) return rank(a) < rank(b);
    if (numeric(a)) return number(a) < number(b);
    if (std::holds_alternative<std::string>(a))
        return std::get<std::string>(a) < std::get<std::string>(b);
    if (std::holds_alternative<std::vector<std::uint8_t>>(a))
        return std::get<std::vector<std::uint8_t>>(a) < std::get<std::vector<std::uint8_t>>(b);
    return false;
}

bool equal(const Value& a, const Value& b) { return !less(a, b) && !less(b, a); }

std::size_t position(const BTreeNode* node, const Value& key) {
    return static_cast<std::size_t>(std::lower_bound(
        node->entries.begin(), node->entries.end(), key,
        [](const IndexEntry& entry, const Value& value) { return less(entry.key, value); })
        - node->entries.begin());
}

void validate_key(const Value& key) {
    if (std::holds_alternative<double>(key) && !std::isfinite(std::get<double>(key)))
        throw std::invalid_argument("Cannot index non-finite numeric value");
}
} // namespace

BTree::BTree(int degree) : minimum_degree(degree) {
    if (degree < 2) throw std::invalid_argument("B-tree minimum degree must be at least 2");
}

bool BTree::empty() const { return row_count == 0; }
std::size_t BTree::size() const { return row_count; }

std::optional<std::vector<RowID>> BTree::search_node(const BTreeNode* node,
                                                      const Value& key) const {
    if (!node) return std::nullopt;
    const auto index = position(node, key);
    if (index < node->entries.size() && equal(node->entries[index].key, key))
        return node->entries[index].row_ids;
    return node->is_leaf ? std::nullopt : search_node(node->children[index].get(), key);
}

std::optional<std::vector<RowID>> BTree::search(const Value& key) const {
    validate_key(key);
    return search_node(root.get(), key);
}

void BTree::split_child(BTreeNode* parent, std::size_t child_index) {
    auto* child = parent->children[child_index].get();
    auto sibling = std::make_unique<BTreeNode>();
    sibling->is_leaf = child->is_leaf;
    const auto middle = static_cast<std::size_t>(minimum_degree - 1);
    IndexEntry promoted = std::move(child->entries[middle]);
    sibling->entries.assign(std::make_move_iterator(child->entries.begin() + middle + 1),
                            std::make_move_iterator(child->entries.end()));
    child->entries.resize(middle);
    if (!child->is_leaf) {
        sibling->children.assign(std::make_move_iterator(child->children.begin() + middle + 1),
                                 std::make_move_iterator(child->children.end()));
        child->children.resize(middle + 1);
    }
    parent->entries.insert(parent->entries.begin() + child_index, std::move(promoted));
    parent->children.insert(parent->children.begin() + child_index + 1, std::move(sibling));
}

void BTree::insert_non_full(BTreeNode* node, const IndexEntry& entry) {
    auto index = position(node, entry.key);
    if (index < node->entries.size() && equal(node->entries[index].key, entry.key)) {
        node->entries[index].row_ids.push_back(entry.row_ids.front());
        return;
    }
    if (node->is_leaf) {
        node->entries.insert(node->entries.begin() + index, entry);
        return;
    }
    if (node->children[index]->entries.size() == static_cast<std::size_t>(2 * minimum_degree - 1)) {
        split_child(node, index);
        if (equal(node->entries[index].key, entry.key)) {
            node->entries[index].row_ids.push_back(entry.row_ids.front());
            return;
        }
        if (less(node->entries[index].key, entry.key)) ++index;
    }
    insert_non_full(node->children[index].get(), entry);
}

void BTree::insert(const Value& key, RowID row_id) {
    validate_key(key);
    if (!root) root = std::make_unique<BTreeNode>();
    if (root->entries.size() == static_cast<std::size_t>(2 * minimum_degree - 1)) {
        auto new_root = std::make_unique<BTreeNode>();
        new_root->is_leaf = false;
        new_root->children.push_back(std::move(root));
        split_child(new_root.get(), 0);
        root = std::move(new_root);
    }
    insert_non_full(root.get(), IndexEntry{key, {row_id}});
    ++row_count;
}
