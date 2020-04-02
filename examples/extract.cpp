#include <fat16/fat16.h>

#include <string>
#include <vector>

#if COMPILE_CPP17
#include <filesystem>
namespace fs = std::filesystem;
#elif COMPILE_CPP14
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

static void extract_file(Fat16::Image &img, Fat16::Entry &entry, const fs::path &path) {
    const auto filename = path / entry.get_filename();

    FILE *f;

#ifdef WIN32
    _wfopen_s(&f, filename.wstring().c_str(), L"wb");
#else
    f = fopen(filename.string().c_str(), "wb");
#endif

    static constexpr std::uint32_t CHUNK_SIZE = 0x10000;

    std::vector<std::uint8_t> temp_buf;
    temp_buf.resize(CHUNK_SIZE);

    std::uint32_t size_left = entry.entry.file_size;
    std::uint32_t offset = 0;

    while (size_left != 0) {
        std::uint32_t size_to_take = std::min<std::uint32_t>(CHUNK_SIZE, size_left);
        if (img.read_from_cluster(&temp_buf[0], offset, entry.entry.starting_cluster, size_to_take) != size_to_take) {
            break;
        }

        size_left -= size_to_take;
        offset += size_to_take;

        fwrite(&temp_buf[0], 1, size_to_take, f);
    }

    fclose(f);
}

static void traverse_directory(Fat16::Image &img, Fat16::Entry &mee, const fs::path &dir_path) {
    fs::create_directories(dir_path);

    while (img.get_next_entry(mee)) {
        if (mee.entry.file_attributes & (int)Fat16::EntryAttribute::DIRECTORY) {
            // Also check if it's not the back folder (. and ..)
            // This can be done by gathering the name
            if (mee.entry.get_entry_type_from_filename() != Fat16::EntryType::DIRECTORY) {
                Fat16::Entry baby;
                if (!img.get_first_entry_dir(mee, baby))
                    break;

                const std::string dir_name = mee.get_filename();
                traverse_directory(img, baby, dir_path / dir_name);
            }
        }

        if (mee.entry.file_attributes & (int)Fat16::EntryAttribute::ARCHIVE) {
            extract_file(img, mee, dir_path);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return false;
    }

    const fs::path file_path{ argv[1] };

    FILE *f;

#ifdef WIN32
    _wfopen_s(&f, file_path.wstring().c_str(), L"rb");
#else
    f = fopen(file_path.string().c_str(), "rb");
#endif

    Fat16::Image img(f,
        // Read hook
        [](void *userdata, void *buffer, std::uint32_t size) -> std::uint32_t {
            return static_cast<std::uint32_t>(fread(buffer, 1, size, (FILE*)userdata));
        },
        // Seek hook
        [](void *userdata, std::uint32_t offset, int mode) -> std::uint32_t {
        fseek((FILE*)userdata, offset, (mode == Fat16::IMAGE_SEEK_MODE_BEG ? SEEK_SET :
            (mode == Fat16::IMAGE_SEEK_MODE_CUR ? SEEK_CUR : SEEK_END)));

        return ftell((FILE*)userdata);
    });

    Fat16::Entry first;

    traverse_directory(img, first, file_path.parent_path() / file_path.stem());

    fclose(f);
}
