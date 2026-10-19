#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ab {

namespace fs = std::filesystem;

// Decoded image pixel buffer (RGBA8) with dimensions.
struct Image {
    std::string path;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;  // RGBA, width*height*4
};

class ImageStore {
public:
    static bool isImageFile(const fs::path& p);

    // Scans a folder for supported image files (sorted by filename).
    void rescan(const fs::path& folder);

    const fs::path& root() const { return m_root; }
    bool hasRoot() const { return !m_root.empty(); }

    // Absolute paths of discovered images.
    const std::vector<fs::path>& images() const { return m_images; }

    // Decodes an image file into RGBA. Returns null on failure.
    std::shared_ptr<Image> load(const fs::path& path);

private:
    fs::path m_root;
    std::vector<fs::path> m_images;
};

}  // namespace ab