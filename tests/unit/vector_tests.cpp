#include "database/database.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    Table documents;
    documents.schema.add_column({"title", "TEXT", "TEXT", false});
    documents.schema.add_column({"topic", "TEXT", "TEXT", false});
    documents.rows = {
        {std::string{"A"}, std::string{"science"}},
        {std::string{"B"}, std::string{"history"}},
        {std::string{"C"}, std::string{"science"}}
    };
    Database db;
    assert(db.create_table("Documents", documents));
    assert(db.create_vector_index("Documents", 2));
    assert(!db.create_vector_index("Documents", 2));
    assert(db.set_embedding("Documents", 0, {1, 0}));
    assert(db.set_embedding("Documents", 1, {0, 1}));
    assert(db.set_embedding("Documents", 2, {-1, 0}));
    auto matches = db.vector_search("Documents", {1, 0}, 3);
    assert(matches.size() == 3);
    assert(matches[0].row_id == 0 && matches[0].similarity == 1.0);
    assert(matches[1].row_id == 1 && matches[1].similarity == 0.0);
    assert(matches[2].row_id == 2 && matches[2].similarity == -1.0);
    matches = db.vector_search("Documents", {1, 0}, 2,
                               Condition{"topic", '=', std::string{"science"}});
    assert(matches.size() == 2 && matches[0].row_id == 0 && matches[1].row_id == 2);
    assert(db.vector_search("Documents", {1, 0}, 0).empty());
    try { (void)db.set_embedding("Documents", 0, {0, 0}); assert(false); }
    catch (const std::invalid_argument&) {}
    try { (void)db.vector_search("Documents", {1, 0, 0}, 1); assert(false); }
    catch (const std::invalid_argument&) {}
    try { (void)db.vector_search("Documents", {1, 0}, 1,
                                Condition{"missing", '=', std::string{"x"}}); assert(false); }
    catch (const std::runtime_error&) {}
    assert(!db.set_embedding("Documents", 3, {1, 0}));
    assert(db.drop_table("Documents"));
    try { (void)db.vector_search("Documents", {1, 0}, 1); assert(false); }
    catch (const std::runtime_error&) {}
    std::cout << "Vector tests passed.\n";
}
