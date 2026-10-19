#pragma once

#include <string>

namespace ab {

// A single combat participant in the initiative order.
struct Participant {
    int id = 0;             // UI/internal identifier (not persisted)
    std::string name;
    int initiative = 0;     // initiative roll (higher acts first)
    int hp = 0;             // current hit points
    int maxHp = 0;
    std::string status;     // conditions/notes, e.g. "poisoned, prone"
};

}  // namespace ab