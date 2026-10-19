#include "projection/ImageStore.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cctype>

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

    if (folder.empty() || !fs::exists(folder) || !fs::is_directory(folder)) {
        return;
    }

    std::error_code ec;
    for (const auto& entry :
         fs::directory_iterator(folder, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (entry.is_regular_file(ec) && !ec && isImageFile(entry.path())) {
            m_images.push_back(entry.path());
        }
        ec.clear();
    }

    std::sort(m_images.begin(), m_images.end());
}

std::shared_ptr<Image> ImageStore::load(const fs::path& path) {
    stbi_set_flip_vertically_on_load(1);

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) {
        return nullptr;
    }

    auto img = std::make_shared<Image>();
    img->path = path.string();
    img->width = w;
    img->height = h;
    img->pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return img;
}

}  // namespace ab