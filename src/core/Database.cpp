#include "core/Database.h"

namespace ab {

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    close();

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = m_db ? sqlite3_errmsg(m_db) : "sqlite3_open failed";
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // Enable WAL + foreign keys for robustness.
    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA foreign_keys = ON;");
    return true;
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::ensureSchema() {
    return exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");");
}

bool Database::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        m_lastError = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::queryInt(const std::string& sql, sqlite3_int64& out) {
    bool found = false;
    bool ok = query(sql, [&](sqlite3_stmt* stmt) {
        out = sqlite3_column_int64(stmt, 0);
        found = true;
        return true;
    });
    return ok && found;
}

bool Database::query(const std::string& sql,
                     const std::function<bool(sqlite3_stmt*)>& row) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return false;
    }

    bool ok = true;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (row && !row(stmt)) {
            break;
        }
    }
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        m_lastError = sqlite3_errmsg(m_db);
        ok = false;
    }

    sqlite3_finalize(stmt);
    return ok;
}

}  // namespace ab