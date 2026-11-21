#include "projection/ImageStore.h"

#include "platform/Platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace ab {

bool ImageStore::isImageFile(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tga" || ext == ".gif";
}

void ImageStore::rescan(const fs::path& folder) {
    m_root = folder;
    m_images.clear();

    if (folder.empty() || !m_storage) {
        return;
    }

    std::vector<Storage::Item> items;
    if (!m_storage->scanFlat(folder, items)) {
        return;
    }
    for (const auto& item : items) {
        if (isImageFile(fs::path(item.name))) {
            m_images.push_back(item.location);
        }
    }

    std::sort(m_images.begin(), m_images.end());
}

std::shared_ptr<Image> ImageStore::load(const fs::path& path) {
    stbi_set_flip_vertically_on_load(1);

    std::vector<unsigned char> bytes;
    if (!m_storage || !m_storage->readBytes(path, bytes) || bytes.empty()) {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* data = stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!data) {
        return nullptr;
    }

    auto img = std::make_shared<Image>();
    img->path = platform::pathToUtf8(path);
    img->width = w;
    img->height = h;
    img->pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return img;
}

}  // namespace ab