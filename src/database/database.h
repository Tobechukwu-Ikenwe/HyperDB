#pragma once

#include "Indexes/btree.h"
#include "Indexes/vector_index.h"
#include "TableData/table.h"
#include "condition.h"
#include "hyperdb/memory.h"

#include <filesystem>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <unordered_map>

class Database {
  private:
    struct IndexData {
        BTree tree;
        int category = 0; // 0: all NULL, 1: numeric, 2: text, 3: blob

        explicit IndexData(std::pmr::memory_resource *resource = std::pmr::get_default_resource())
            : tree(3, resource) {}
    };
    using IndexMap = std::pmr::unordered_map<std::string, IndexData>;

    std::shared_ptr<hyperdb::MemoryResource> memory_resource_owner_;
    std::pmr::unordered_map<std::string, Table> tables_;
    std::pmr::unordered_map<std::string, IndexMap> indexes_;
    std::pmr::unordered_map<std::string, VectorIndex> vector_indexes_;

  public:
    Database();
    explicit Database(hyperdb::AllocatorConfig allocator);
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    Database(Database &&other) noexcept;
    Database &operator=(Database &&) = delete;

    [[nodiscard]] hyperdb::MemoryResource &memory_resource() const noexcept {
        return *memory_resource_owner_;
    }
    [[nodiscard]] hyperdb::AllocatorStats allocator_stats() const noexcept {
        return memory_resource_owner_->stats();
    }

    bool create_table(const std::string &table_name, const Table &table);

    const Table *find_table(const std::string &table_name) const;

    // Inserts one row after checking schema width and required values.
    // Returns false when the table does not exist.
    bool insert_row(const std::string &table_name, Row row);

    // Replaces rows after validation. old_row_ids maps each new row to its former
    // row ID (strictly increasing), preserving attached embeddings on survivors.
    bool replace_rows(const std::string &table_name, std::vector<Row> rows,
                      const std::vector<RowID> &old_row_ids);

    // One equality index per column. Missing table/column or duplicate index returns false.
    bool create_index(const std::string &table_name, const std::string &column_name);

    // nullopt means no usable index; an empty vector means an indexed miss.
    std::optional<std::vector<RowID>> lookup_index(const std::string &table_name,
                                                   const std::string &column_name,
                                                   const Value &key) const;

    bool create_vector_index(const std::string &table_name, std::size_t dimensions);
    bool set_embedding(const std::string &table_name, RowID row_id, std::vector<double> embedding);
    std::vector<VectorMatch>
    vector_search(const std::string &table_name, const std::vector<double> &query,
                  std::size_t limit,
                  const std::optional<Condition> &metadata_filter = std::nullopt) const;

    bool drop_table(const std::string &table_name);

    std::size_t table_count() const;

    // Saves a versioned snapshot beside the destination and replaces it on success.
    // POSIX builds sync the file and containing directory around the rename.
    void save(const std::filesystem::path &path) const;
    static Database load(const std::filesystem::path &path,
                         hyperdb::AllocatorConfig allocator = {});

  private:
    explicit Database(std::shared_ptr<hyperdb::MemoryResource> memory_resource);
};
