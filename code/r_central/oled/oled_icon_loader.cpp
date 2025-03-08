#include "oled_icon_loader.h"
#include <fstream>
#include <cstdint>
#include "../../base/base.h"

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
    uint32_t info_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
};
#pragma pack(pop)

std::vector<uint8_t> OLEDIconLoader::pack_pixels(const std::vector<std::vector<bool>> &pixels) const
{
    const int height = pixels.size();
    const int width = height > 0 ? pixels[0].size() : 0;
    const int bytes_per_row = (width + 7) / 8;

    std::vector<uint8_t> packed;
    packed.reserve(height * bytes_per_row);

    for (const auto &row : pixels)
    {
        for (int x = 0; x < width; x += 8)
        {
            uint8_t byte = 0;
            for (int bit = 0; bit < 8; ++bit)
            {
                if (x + bit < width && row[x + bit])
                {
                    byte |= (1 << (7 - bit));
                }
            }
            packed.push_back(byte);
        }
    }
    return packed;
}

OLEDIcon OLEDIconLoader::parse_bmp(const std::string &filename) const
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        log_error_and_alarm("[OLED] Can't open file: %s", filename.c_str());
    }

    BMPHeader header;
    file.read(reinterpret_cast<char *>(&header), sizeof(BMPHeader));

    if (header.type != 0x4D42)
    {
        log_error_and_alarm("[OLED] It's not standard BMP file: %s", filename.c_str());
    }
    if (header.bit_count != 1)
    {
        log_error_and_alarm("[OLED] Only support 1 bit BMP file: %s", filename.c_str());
    }
    if (header.width > 128 || header.height > 64)
    {
        log_error_and_alarm("[OLED] Resolution over the 128x64 limit: %s", filename.c_str());
    }

    // 读取颜色表
    uint32_t color_table[2];
    file.seekg(sizeof(BMPHeader), std::ios::beg);
    file.read(reinterpret_cast<char *>(color_table), 8);

    const bool white_is_zero = (color_table[0] == 0x00FFFFFF);

    file.seekg(header.offset, std::ios::beg);

    OLEDIcon icon;
    icon.width = header.width;
    icon.height = header.height;
    icon.pixels.resize(header.height, std::vector<bool>(header.width));

    const int row_stride = ((header.width + 31) / 32) * 4;

    for (int y = header.height - 1; y >= 0; --y)
    {
        std::vector<uint8_t> row_data(row_stride);
        file.read(reinterpret_cast<char *>(row_data.data()), row_stride);

        for (int x = 0; x < header.width; ++x)
        {
            const int byte_index = x / 8;
            const int bit_index = 7 - (x % 8);

            const bool bit_value = (row_data[byte_index] >> bit_index) & 1;
            icon.pixels[y][x] = white_is_zero ? bit_value : !bit_value;
        }
    }

    icon.packed = pack_pixels(icon.pixels);
    return icon;
}

int OLEDIconLoader::load_icons(const std::string &directory)
{
    const std::string prefix = "icon_oled_";
    const std::string suffix = ".bmp";

    DIR *dir = opendir(directory.c_str());
    if (!dir)
    {
        log_error_and_alarm("[OLED] Can't open folder: %s", directory.c_str());
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        const std::string filename = entry->d_name;

        if (OLEDIconLoader::string_starts_with(filename, prefix) && OLEDIconLoader::string_ends_with(filename, suffix))
        {
            const std::string icon_name = filename.substr(
                prefix.length(),
                filename.length() - prefix.length() - suffix.length());

            try
            {
                OLEDIcon icon = parse_bmp(directory + "/" + filename);
                icon_cache_.emplace(icon_name, std::move(icon));
            }
            catch (const std::exception &e)
            {
                log_error_and_alarm("[OLED] Parse icon failed: %s-%s", filename.c_str(), e.what());
            }
        }
    }

    closedir(dir);
    return icon_cache_.size();
}

const OLEDIcon& OLEDIconLoader::get_icon(const std::string &name) const
{
    auto it = icon_cache_.find(name);
    if (it == icon_cache_.end())
    {
        log_error_and_alarm("[OLED] Icon: %s not in cache.", name.c_str());
    }
    return it->second;
}

OLEDIconLoader& OLEDIconLoader::get_instance()
{
    static OLEDIconLoader instance;
    return instance;
}

bool OLEDIconLoader::string_starts_with(const std::string &str, const std::string &prefix)
{
    if (str.size() < prefix.size())
    {
        return false;
    }
    return str.substr(0, prefix.size()) == prefix;
}

bool OLEDIconLoader::string_ends_with(const std::string &str, const std::string &suffix)
{
    if (str.size() < suffix.size())
    {
        return false;
    }
    return str.substr(str.size() - suffix.size()) == suffix;
}