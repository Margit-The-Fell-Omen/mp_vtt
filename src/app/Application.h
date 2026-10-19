#pragma once

#include <string>

namespace ab {

struct Settings {
    std::string lastProjectPath;
    int windowWidth = 1280;
    int windowHeight = 720;
};

class Application {
public:
    Application();
    ~Application();

    int run();

private:
    bool init();
    void shutdown();
    void newFrame();

    Settings m_settings;
    void* m_window = nullptr;  // GLFWwindow*
};

}  // namespace ab