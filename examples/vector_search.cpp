#include "database/database.h"

#include <iostream>
#include <string>

int main() {
    Table documents;
    documents.schema.add_column({"title", "TEXT", "TEXT", false});
    documents.schema.add_column({"topic", "TEXT", "TEXT", false});
    documents.rows = {
        {std::string{"Database indexes"}, std::string{"systems"}},
        {std::string{"Neural embeddings"}, std::string{"ai"}},
        {std::string{"Query planning"}, std::string{"systems"}}
    };
    Database db;
    db.create_table("Documents", documents);
    db.create_vector_index("Documents", 2);
    // Illustrative vectors supplied by the caller. No model is run by HyperDB.
    db.set_embedding("Documents", 0, {0.9, 0.1});
    db.set_embedding("Documents", 1, {0.0, 1.0});
    db.set_embedding("Documents", 2, {1.0, 0.0});
    const auto results = db.vector_search("Documents", {1.0, 0.0}, 2,
        Condition{"topic", '=', std::string{"systems"}});
    const auto* table = db.find_table("Documents");
    for (const auto& match : results)
        std::cout << std::get<std::string>(table->rows[match.row_id][0])
                  << '\t' << match.similarity << '\n';
}
