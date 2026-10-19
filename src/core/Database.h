#pragma once

#include <sqlite3.h>

#include <string>
#include <vector>
#include <functional>

namespace ab {

// Thin RAII wrapper around a SQLite connection with a small helpers API.
class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Opens (creating if necessary) the database at the given path.
    // Returns true on success; error message retrievable via lastError().
    bool open(const std::string& path);

    bool isOpen() const { return m_db != nullptr; }
    void close();

    // Runs one or more SQL statements (no bound parameters, no result rows).
    bool exec(const std::string& sql);

    // Creates core tables if they do not exist (idempotent, versioned).
    bool ensureSchema();

    // Convenience: fetch a single integer scalar (e.g. SELECT COUNT(*)).
    // Returns false if the query produced no rows.
    bool queryInt(const std::string& sql, sqlite3_int64& out);

    // Executes arbitrary SQL with an optional row callback.
    // callback returns true to keep iterating.
    bool query(const std::string& sql,
               const std::function<bool(sqlite3_stmt*)>& row = nullptr);

    std::string lastError() const { return m_lastError; }

    sqlite3* handle() { return m_db; }

private:
    sqlite3* m_db = nullptr;
    std::string m_lastError;
};

}  // namespace ab