#include "initiative/InitiativeTracker.h"

#include "core/Database.h"

#include <sqlite3.h>

#include <algorithm>

namespace ab {

namespace {
constexpr const char* kSchemaParticipant =
    "CREATE TABLE IF NOT EXISTS initiative_participants ("
    "  sort_order INTEGER PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  initiative INTEGER NOT NULL DEFAULT 0,"
    "  hp INTEGER NOT NULL DEFAULT 0,"
    "  max_hp INTEGER NOT NULL DEFAULT 0,"
    "  status TEXT NOT NULL DEFAULT ''"
    ");";

constexpr const char* kSchemaMeta =
    "CREATE TABLE IF NOT EXISTS initiative_meta ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL"
    ");";
}  // namespace

int InitiativeTracker::allocateId() {
    return m_nextId++;
}

int InitiativeTracker::addParticipant(const std::string& name) {
    Participant p;
    p.id = allocateId();
    p.name = name;
    m_participants.push_back(std::move(p));
    if (m_current < 0) {
        m_current = 0;
    }
    return p.id;
}

void InitiativeTracker::removeParticipant(int id) {
    auto it = std::find_if(m_participants.begin(), m_participants.end(),
                           [id](const Participant& p) { return p.id == id; });
    if (it == m_participants.end()) {
        return;
    }
    size_t removedIndex = static_cast<size_t>(std::distance(m_participants.begin(), it));
    m_participants.erase(it);

    if (m_participants.empty()) {
        m_current = -1;
    } else if (static_cast<int>(removedIndex) < m_current) {
        --m_current;
    } else if (m_current >= static_cast<int>(m_participants.size())) {
        m_current = 0;
    }
}

void InitiativeTracker::sortByInitiative() {
    std::stable_sort(m_participants.begin(), m_participants.end(),
                     [](const Participant& a, const Participant& b) {
                         if (a.initiative != b.initiative) {
                             return a.initiative > b.initiative;  // high first
                         }
                         return a.name < b.name;
                     });
    m_current = m_participants.empty() ? -1 : 0;
}

void InitiativeTracker::nextTurn() {
    if (m_participants.empty()) {
        return;
    }
    if (m_current < 0 || m_current + 1 >= static_cast<int>(m_participants.size())) {
        m_current = 0;
        ++m_round;
    } else {
        ++m_current;
    }
}

void InitiativeTracker::previousTurn() {
    if (m_participants.empty()) {
        return;
    }
    if (m_current <= 0) {
        m_current = static_cast<int>(m_participants.size()) - 1;
        if (m_round > 1) {
            --m_round;
        }
    } else {
        --m_current;
    }
}

void InitiativeTracker::restartRound() {
    if (m_participants.empty()) {
        m_current = -1;
    } else {
        m_current = 0;
    }
}

void InitiativeTracker::setRound(int round) {
    if (round >= 1) {
        m_round = round;
    }
}

void InitiativeTracker::adjustHp(int index, int delta) {
    if (index < 0 || index >= static_cast<int>(m_participants.size())) {
        return;
    }
    Participant& p = m_participants[index];
    p.hp += delta;
    if (p.hp < 0) {
        p.hp = 0;
    }
    if (p.maxHp > 0 && p.hp > p.maxHp) {
        p.hp = p.maxHp;
    }
}

void InitiativeTracker::setMaxHp(int index, int maxHp) {
    if (index < 0 || index >= static_cast<int>(m_participants.size())) {
        return;
    }
    Participant& p = m_participants[index];
    p.maxHp = maxHp < 0 ? 0 : maxHp;
    if (p.maxHp > 0 && p.hp > p.maxHp) {
        p.hp = p.maxHp;
    }
}

bool InitiativeTracker::save(Database& db) const {
    if (!db.exec(kSchemaParticipant)) {
        return false;
    }
    if (!db.exec(kSchemaMeta)) {
        return false;
    }
    if (!db.exec("DELETE FROM initiative_participants;")) {
        return false;
    }

    sqlite3* handle = db.handle();
    const char* insertSql =
        "INSERT INTO initiative_participants"
        "(sort_order, name, initiative, hp, max_hp, status)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    for (size_t i = 0; i < m_participants.size(); ++i) {
        const Participant& p = m_participants[i];
        sqlite3_bind_int(stmt, 1, static_cast<int>(i));
        sqlite3_bind_text(stmt, 2, p.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, p.initiative);
        sqlite3_bind_int(stmt, 4, p.hp);
        sqlite3_bind_int(stmt, 5, p.maxHp);
        sqlite3_bind_text(stmt, 6, p.status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);

    auto saveMeta = [&](const std::string& key, const std::string& value) {
        std::string sql =
            "INSERT INTO initiative_meta(key, value) VALUES(?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) {
            return;
        }
        sqlite3_bind_text(s, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(s);
        sqlite3_finalize(s);
    };
    saveMeta("round", std::to_string(m_round));
    saveMeta("current", std::to_string(m_current));
    return true;
}

bool InitiativeTracker::load(Database& db) {
    db.exec(kSchemaParticipant);
    db.exec(kSchemaMeta);

    m_participants.clear();
    m_nextId = 1;

    bool ok = db.query(
        "SELECT name, initiative, hp, max_hp, status FROM "
        "initiative_participants ORDER BY sort_order;",
        [this](sqlite3_stmt* stmt) {
            Participant p;
            p.id = allocateId();
            const unsigned char* n = sqlite3_column_text(stmt, 0);
            p.name = n ? reinterpret_cast<const char*>(n) : "";
            p.initiative = sqlite3_column_int(stmt, 1);
            p.hp = sqlite3_column_int(stmt, 2);
            p.maxHp = sqlite3_column_int(stmt, 3);
            const unsigned char* s = sqlite3_column_text(stmt, 4);
            p.status = s ? reinterpret_cast<const char*>(s) : "";
            m_participants.push_back(std::move(p));
            return true;
        });

    sqlite3_int64 round = 1;
    if (db.queryInt("SELECT value FROM initiative_meta WHERE key = 'round';",
                    round)) {
        m_round = round > 0 ? static_cast<int>(round) : 1;
    }

    sqlite3_int64 current = -1;
    if (db.queryInt("SELECT value FROM initiative_meta WHERE key = 'current';",
                    current)) {
        m_current = static_cast<int>(current);
    }
    if (m_current >= static_cast<int>(m_participants.size())) {
        m_current = m_participants.empty() ? -1 : 0;
    }

    return ok;
}

}  // namespace ab