#include "vector_index.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
long double norm(const std::vector<double>& values, std::size_t dimensions) {
    if (values.size() != dimensions) throw std::invalid_argument("Embedding dimension mismatch");
    long double squared = 0;
    for (double value : values) {
        if (!std::isfinite(value)) throw std::invalid_argument("Embedding must be finite");
        squared += static_cast<long double>(value) * value;
    }
    const auto result = std::sqrt(squared);
    if (result == 0 || !std::isfinite(result))
        throw std::invalid_argument("Embedding must have finite nonzero norm");
    return result;
}
} // namespace

VectorIndex::VectorIndex(std::size_t dimensions) : dimensions_(dimensions) {
    if (dimensions == 0) throw std::invalid_argument("Embedding dimension must be positive");
}

void VectorIndex::upsert(RowID row_id, std::vector<double> embedding) {
    (void)norm(embedding, dimensions_);
    embeddings_.insert_or_assign(row_id, std::move(embedding));
}

std::vector<VectorMatch> VectorIndex::search(const std::vector<double>& query,
    std::size_t limit, const std::function<bool(RowID)>& include) const {
    const auto query_norm = norm(query, dimensions_);
    if (limit == 0) return {};
    std::vector<VectorMatch> matches;
    matches.reserve(embeddings_.size());
    for (const auto& [row_id, embedding] : embeddings_) {
        if (include && !include(row_id)) continue;
        long double dot = 0;
        for (std::size_t i = 0; i < dimensions_; ++i)
            dot += static_cast<long double>(query[i]) * embedding[i];
        const auto similarity = static_cast<double>(dot / (query_norm * norm(embedding, dimensions_)));
        matches.push_back({row_id, similarity});
    }
    const auto better = [](const VectorMatch& a, const VectorMatch& b) {
        return a.similarity == b.similarity ? a.row_id < b.row_id : a.similarity > b.similarity;
    };
    if (matches.size() > limit) {
        std::partial_sort(matches.begin(), matches.begin() + limit, matches.end(), better);
        matches.resize(limit);
    } else {
        std::sort(matches.begin(), matches.end(), better);
    }
    return matches;
}
