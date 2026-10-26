#include "database/database.h"
#include "database/TableData/column.h"
#include "database/TableData/table.h"
#include "QueryExecutor/query_executor.h"

#include <cstdint>
#include <filesystem>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef __unix__
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
class SnapshotLock {
public:
    explicit SnapshotLock(const std::filesystem::path& snapshot) {
#ifdef __unix__
        auto lock_path = snapshot;
        lock_path += ".lock";
        fd_ = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd_ < 0) throw std::system_error(errno, std::generic_category(), "Open snapshot lock");
        if (::flock(fd_, LOCK_EX) != 0) {
            const int error = errno;
            ::close(fd_);
            throw std::system_error(error, std::generic_category(), "Lock snapshot");
        }
#else
        (void)snapshot;
        throw std::runtime_error("Snapshot CLI requires POSIX file locking");
#endif
    }
    SnapshotLock(const SnapshotLock&) = delete;
    SnapshotLock& operator=(const SnapshotLock&) = delete;
    ~SnapshotLock() {
#ifdef __unix__
        if (fd_ >= 0) ::close(fd_);
#endif
    }
private:
    int fd_ = -1;
};

void print_value(const Value& value) {
    std::visit([](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) std::cout << "NULL";
        else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) std::cout << "<blob>";
        else std::cout << item;
    }, value);
}
void print_result(const QueryResult& result) {
    for (std::size_t i = 0; i < result.columns.size(); ++i)
        std::cout << (i ? "\t" : "") << result.columns[i];
    std::cout << '\n';
    for (const auto& row : result.rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i) std::cout << '\t';
            print_value(row[i]);
        }
        std::cout << '\n';
    }
}
void print_json_string(const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::cout << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') std::cout << '\\' << c;
        else if (c < 0x20) std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        else std::cout << c;
    }
    std::cout << '"';
}
void print_json_value(const Value& value) {
    std::visit([](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) std::cout << "null";
        else if constexpr (std::is_same_v<T, std::string>) print_json_string(item);
        else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
            static constexpr char hex[] = "0123456789abcdef";
            std::string bytes = "0x";
            for (auto byte : item) { bytes += hex[byte >> 4]; bytes += hex[byte & 15]; }
            print_json_string(bytes);
        } else if constexpr (std::is_same_v<T, double>) {
            if (!std::isfinite(item)) throw std::runtime_error("Non-finite result number");
            std::cout.precision(std::numeric_limits<double>::max_digits10);
            std::cout << item;
        } else std::cout << item;
    }, value);
}
void print_json_result(const StatementResult& result) {
    if (result.is_write) {
        std::cout << "{\"kind\":\"write\",\"rows_affected\":" << result.rows_affected
                  << ",\"modified\":" << (result.modified ? "true" : "false") << "}\n";
        return;
    }
    std::cout << "{\"kind\":\"query\",\"columns\":[";
    for (std::size_t i = 0; i < result.query.columns.size(); ++i) {
        if (i) std::cout << ',';
        print_json_string(result.query.columns[i]);
    }
    std::cout << "],\"rows\":[";
    for (std::size_t i = 0; i < result.query.rows.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << '[';
        for (std::size_t j = 0; j < result.query.rows[i].size(); ++j) {
            if (j) std::cout << ',';
            print_json_value(result.query.rows[i][j]);
        }
        std::cout << ']';
    }
    std::cout << "]}\n";
}
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string{argv[1]} == "--index") {
        if (argc != 5) {
            std::cerr << "Usage: hyperdb --index SNAPSHOT TABLE COLUMN\n";
            return 2;
        }
        try {
            SnapshotLock lock(argv[2]);
            auto database = Database::load(argv[2]);
            if (!database.create_index(argv[3], argv[4]))
                throw std::runtime_error("Cannot create index: missing table/column or duplicate index");
            database.save(argv[2]);
            std::cout << "Index created\n";
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }
    if (argc >= 2 && std::string{argv[1]} == "--init") {
        if (argc != 3) {
            std::cerr << "Usage: hyperdb --init SNAPSHOT\n";
            return 2;
        }
        try {
            SnapshotLock lock(argv[2]);
            if (std::filesystem::exists(argv[2]))
                throw std::runtime_error("Snapshot already exists");
            Database{}.save(argv[2]);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }
    if (argc >= 2 && (std::string{argv[1]} == "--db" ||
                      std::string{argv[1]} == "--db-json")) {
        const bool json = std::string{argv[1]} == "--db-json";
        if (argc != 4) {
            std::cerr << "Usage: hyperdb --db|--db-json SNAPSHOT 'SQL statement'\n";
            return 2;
        }
        try {
            SnapshotLock lock(argv[2]);
            auto database = Database::load(argv[2]);
            const auto result = execute_statement(database, argv[3]);
            if (json) {
                if (result.is_write && result.modified) database.save(argv[2]);
                print_json_result(result);
                return 0;
            }
            if (result.is_write) {
                if (result.modified) database.save(argv[2]);
                std::cout << "OK (" << result.rows_affected <<
                    (result.rows_affected == 1 ? " row affected)\n" : " rows affected)\n");
            } else print_result(result.query);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }
    Table department;

    department.schema.add_column({
        "Department_ID",
        "INTEGER",
        "INTEGER",
        false,
        true,
        false,
        std::monostate{}
    });

    department.schema.add_column({
        "Name",
        "TEXT",
        "TEXT",
        false,
        false,
        false,
        std::monostate{}
    });

    department.schema.add_column({
        "Budget_in_Billions",
        "REAL",
        "REAL",
        false,
        false,
        false,
        std::monostate{}
    });

    department.rows.push_back({
        std::int64_t{1},
        std::string{"Finance"},
        5.2
    });

    department.rows.push_back({
        std::int64_t{2},
        std::string{"Biology"},
        3.7
    });

    department.rows.push_back({
        std::int64_t{3},
        std::string{"Physics"},
        6.1
    });

    Database database;

    if (!database.create_table("Department", department)) {
        std::cerr << "Failed to create Department table\n";
        return 1;
    }

    if (argc >= 2 && std::string{argv[1]} == "--save-demo") {
        if (argc != 3) {
            std::cerr << "Usage: hyperdb --save-demo SNAPSHOT\n";
            return 2;
        }
        try {
            SnapshotLock lock(argv[2]);
            database.save(argv[2]);
        }
        catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }

    const std::string sql = argc > 1 ? argv[1]
        : "SELECT Name, Budget_in_Billions FROM Department WHERE Budget_in_Billions > 4 ORDER BY Budget_in_Billions DESC LIMIT 2";
    try {
        const auto result = execute_statement(database, sql);
        if (result.is_write) std::cout << "OK (" << result.rows_affected <<
            (result.rows_affected == 1 ? " row affected)\n" : " rows affected)\n");
        else print_result(result.query);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
