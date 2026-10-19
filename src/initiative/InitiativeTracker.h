#pragma once

#include "initiative/Participant.h"

#include <string>
#include <vector>

namespace ab {

class Database;

// Manages a single combat encounter: participant list, initiative order,
// round/turn progression and persistence to SQLite.
class InitiativeTracker {
public:
    int addParticipant(const std::string& name);
    void removeParticipant(int id);

    // Sorts participants by initiative (descending); highest rolls first.
    void sortByInitiative();

    void nextTurn();      // advance to the next participant (wraps, bumps round)
    void previousTurn();
    void restartRound();  // go back to the first participant, keep round

    void setRound(int round);
    int round() const { return m_round; }
    int currentIndex() const { return m_current; }

    // Adjusts a participant's HP by delta (clamped to [0, maxHp]).
    void adjustHp(int index, int delta);
    void setMaxHp(int index, int maxHp);

    std::vector<Participant>& participants() { return m_participants; }
    const std::vector<Participant>& participants() const { return m_participants; }

    // True if the given index is the active participant.
    bool isCurrent(size_t index) const {
        return static_cast<int>(index) == m_current;
    }

    bool save(Database& db) const;
    bool load(Database& db);

private:
    int allocateId();

    std::vector<Participant> m_participants;
    int m_round = 1;
    int m_current = -1;   // index of active participant (-1 = none)
    int m_nextId = 1;
};

}  // namespace ab