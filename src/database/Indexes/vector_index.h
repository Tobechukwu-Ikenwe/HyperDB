#pragma once

#include "row_id.h"

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

struct VectorMatch {
    RowID row_id;
    double similarity;
};

// Exact cosine search over embeddings attached to stable, append-only row IDs.
class VectorIndex {
public:
    explicit VectorIndex(std::size_t dimensions);
    std::size_t dimensions() const { return dimensions_; }
    std::size_t size() const { return embeddings_.size(); }
    const std::unordered_map<RowID, std::vector<double>>& entries() const { return embeddings_; }
    void upsert(RowID row_id, std::vector<double> embedding);
    std::vector<VectorMatch> search(const std::vector<double>& query, std::size_t limit,
        const std::function<bool(RowID)>& include = {}) const;

private:
    std::size_t dimensions_;
    std::unordered_map<RowID, std::vector<double>> embeddings_;
};
