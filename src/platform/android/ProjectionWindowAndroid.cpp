#include "projection/ProjectionWindow.h"

namespace ab {

// Android projection (Presentation API) is not wired yet. Keep the desktop
// feature surface compilable and report "unavailable" from open().
ProjectionWindow::~ProjectionWindow() = default;

bool ProjectionWindow::open(int monitorIndex) {
    (void)monitorIndex;
    return false;
}

void ProjectionWindow::close() {}

bool ProjectionWindow::closeRequested() const {
    return false;
}

void ProjectionWindow::showImage(const std::shared_ptr<Image>& image) {
    m_image = image;
}

void ProjectionWindow::clearImage() {
    m_image = nullptr;
}

void ProjectionWindow::rotateImage(int quarterTurns) {
    m_rotation = ((m_rotation + quarterTurns) % 4 + 4) % 4;
}

void ProjectionWindow::resetImageTransform() {
    m_rotation = 0;
    m_flipH = false;
    m_flipV = false;
}

void ProjectionWindow::render() {}

}  // namespace ab
