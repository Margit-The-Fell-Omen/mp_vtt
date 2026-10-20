#pragma once

#include <string>

namespace ab {

// Application settings persisted to the SQLite database.
class Settings {
public:
    std::string lastProjectPath;
    std::string lastImagesPath;
    std::string theme = "catppuccin-mocha";
    int windowWidth = 1280;
    int windowHeight = 720;

    // Loads settings from the database. Missing values keep their defaults.
    void load(class Database& db);
    void save(class Database& db) const;
};

}  // namespace ab