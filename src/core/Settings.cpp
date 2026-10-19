#include "core/Settings.h"

#include "core/Database.h"

#include <sqlite3.h>

#include <string>

namespace ab {

namespace {

std::string getString(Database& db, const std::string& key) {
    std::string sql =
        "SELECT value FROM settings WHERE key = '" + key + "';";
    std::string result;
    db.query(sql, [&](sqlite3_stmt* stmt) {
        const unsigned char* v = sqlite3_column_text(stmt, 0);
        if (v) {
            result = reinterpret_cast<const char*>(v);
        }
        return false;
    });
    return result;
}

sqlite3_int64 getInt(Database& db, const std::string& key) {
    std::string sql =
        "SELECT value FROM settings WHERE key = '" + key + "';";
    sqlite3_int64 v = 0;
    db.query(sql, [&](sqlite3_stmt* stmt) {
        v = sqlite3_column_int64(stmt, 0);
        return false;
    });
    return v;
}

void setValue(Database& db, const std::string& key, const std::string& value) {
    // key is trusted (hardcoded keys), value is escaped via prepared binding.
    std::string sql =
        "INSERT INTO settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

}  // namespace

void Settings::load(Database& db) {
    std::string path = getString(db, "last_project_path");
    if (!path.empty()) {
        lastProjectPath = path;
    }

    std::string img = getString(db, "last_images_path");
    if (!img.empty()) {
        lastImagesPath = img;
    }

    auto w = getInt(db, "window_width");
    if (w > 0) {
        windowWidth = static_cast<int>(w);
    }
    auto h = getInt(db, "window_height");
    if (h > 0) {
        windowHeight = static_cast<int>(h);
    }
}

void Settings::save(Database& db) const {
    setValue(db, "last_project_path", lastProjectPath);
    setValue(db, "last_images_path", lastImagesPath);
    setValue(db, "window_width", std::to_string(windowWidth));
    setValue(db, "window_height", std::to_string(windowHeight));
}

}  // namespace ab