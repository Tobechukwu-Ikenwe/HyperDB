#include "database/database.h"
#include <gtest/gtest.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

TEST(VectorTests, ExistingCases) {
    Table documents;
    documents.schema.add_column({"title", "TEXT", "TEXT", false});
    documents.schema.add_column({"topic", "TEXT", "TEXT", false});
    documents.rows = {{std::string{"A"}, std::string{"science"}},
                      {std::string{"B"}, std::string{"history"}},
                      {std::string{"C"}, std::string{"science"}}};
    Database db;
    EXPECT_TRUE(db.create_table("Documents", documents));
    EXPECT_TRUE(db.create_vector_index("Documents", 2));
    EXPECT_TRUE(!db.create_vector_index("Documents", 2));
    EXPECT_TRUE(db.set_embedding("Documents", 0, {1, 0}));
    EXPECT_TRUE(db.set_embedding("Documents", 1, {0, 1}));
    EXPECT_TRUE(db.set_embedding("Documents", 2, {-1, 0}));
    auto matches = db.vector_search("Documents", {1, 0}, 3);
    EXPECT_TRUE(matches.size() == 3);
    EXPECT_TRUE(matches[0].row_id == 0 && matches[0].similarity == 1.0);
    EXPECT_TRUE(matches[1].row_id == 1 && matches[1].similarity == 0.0);
    EXPECT_TRUE(matches[2].row_id == 2 && matches[2].similarity == -1.0);
    matches =
        db.vector_search("Documents", {1, 0}, 2, Condition{"topic", '=', std::string{"science"}});
    EXPECT_TRUE(matches.size() == 2 && matches[0].row_id == 0 && matches[1].row_id == 2);
    EXPECT_TRUE(db.vector_search("Documents", {1, 0}, 0).empty());
    try {
        (void)db.set_embedding("Documents", 0, {0, 0});
        EXPECT_TRUE(false);
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)db.vector_search("Documents", {1, 0, 0}, 1);
        EXPECT_TRUE(false);
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)db.vector_search("Documents", {1, 0}, 1, Condition{"missing", '=', std::string{"x"}});
        EXPECT_TRUE(false);
    } catch (const std::runtime_error &) {
    }
    EXPECT_TRUE(!db.set_embedding("Documents", 3, {1, 0}));
    EXPECT_TRUE(db.drop_table("Documents"));
    try {
        (void)db.vector_search("Documents", {1, 0}, 1);
        EXPECT_TRUE(false);
    } catch (const std::runtime_error &) {
    }
    std::cout << "Vector tests passed.\n";
}
