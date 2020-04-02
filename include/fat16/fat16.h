#pragma once

#include <cstdint>

namespace Fat16 {
    #pragma pack(push, 1)
    struct BootBlock {
        std::uint8_t jump_code[3];
        std::uint8_t manufacturer_description[8];
        std::uint16_t bytes_per_block;
        std::uint8_t num_blocks_per_allocation_unit;
        std::uint16_t num_reserved_blocks;
        std::uint8_t num_fat;                       ///< Number of FAT (File Allocation Table).
        std::uint16_t num_root_dirs;                ///< Number of root directories.
        std::uint16_t num_blocks_in_image_op1;      ///< Total number of block in this images. Option 1
        std::uint8_t media_descriptor;
        std::uint16_t num_blocks_per_fat;           ///< Number of blocks occupied by one FAT.
        std::uint16_t num_blocks_per_track;
        std::uint16_t num_heads;
        std::uint32_t num_hidden_blocks;
        std::uint32_t num_blocks_in_image_op2;      ///< Total number of block in this images. Option 2
        std::uint16_t physical_driver_num;
        std::uint8_t extended_boot_record_signature;
        std::uint32_t volume_sig_num;
        char volume_label[11];
        char file_sys_id[8];
        std::uint8_t bootstrap_code[0x1C0];
        std::uint16_t boot_block_sig;

        std::uint32_t fat_region_start() const;
        std::uint32_t root_directory_region_start() const;
        std::uint32_t data_region_start() const;
    };
    #pragma pack(pop)

    enum class EntryType {
        FILE = 0,
        DIRECTORY = 1,
        DELETED = 2,
        UNUSED = 3
    };

    enum class EntryAttribute {
        NONE = 0,
        READONLY = 0x1,
        HIDDEN = 0x2,
        SYSFILE = 0x4,
        SPECIAL = 0x8,
        DIRECTORY = 0x10,
        ARCHIVE = 0x20,
        LFN = READONLY | HIDDEN | SYSFILE | SPECIAL
    };

    #pragma pack(push, 1)
    struct FundamentalEntry {
        std::uint8_t filename[8] = {};
        char filename_ext[3] = {};
        std::uint8_t file_attributes = 0;
        std::uint8_t reserved[10] = {};
        std::uint16_t last_modified_time = 0;
        std::uint16_t last_modified_date = 0;
        std::uint16_t starting_cluster = 0;
        std::uint32_t file_size = 0;

        const char *get_filename();
        EntryType get_entry_type_from_filename();
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct LongFileNameEntry {
        std::uint8_t position{};
        char16_t name_part_1[5] = {};
        std::uint8_t attrib{};
        std::uint8_t type{};
        std::uint8_t checksum{};
        char16_t name_part_2[6] = {};
        std::uint16_t padding{};
        char16_t name_part_3[2] = {};
    };
    #pragma pack(pop)

    // Comparison operators for LongFileNameEntry
    inline bool operator<(const LongFileNameEntry &lhs, const LongFileNameEntry &rhs) {
        return lhs.position < rhs.position;
    }

    inline bool operator>(const LongFileNameEntry &lhs, const LongFileNameEntry &rhs) {
        return !(lhs < rhs);
    }

    inline bool operator==(const LongFileNameEntry &lhs, const LongFileNameEntry &rhs) {
        return lhs.position == rhs.position;
    }

    inline bool operator!=(const LongFileNameEntry &lhs, const LongFileNameEntry &rhs) {
        return !(lhs == rhs);
    }

    using ClusterID = std::uint16_t; // Numbered from 2

    enum class ExtendedArrayError {
        Success = 0,
        HasEntry,
        Empty,
        InvalidEntry,
        BadPosition,
        CannotResize
    };

    struct Entry {
    private:
        struct Node {
            Node *next{};
            LongFileNameEntry *entry{};

            Node() {
                next = nullptr;
            }

            explicit Node(LongFileNameEntry *new_entry) {
                entry = new_entry;
            }
        };

        friend struct Image;

        std::uint32_t cursor_record{};
        ClusterID root{};
        Node* extended_entries{};
        unsigned int num_entries = 0;

        static Node *create_new(const LongFileNameEntry new_entry) {
            const auto cur = new Node;
            cur->entry = new LongFileNameEntry;
            *cur->entry = new_entry;
            return cur;
        }

        void replace_head() {
            auto cur = extended_entries;
            extended_entries = extended_entries->next;
            delete cur;
            num_entries--;
        }

    public:
        FundamentalEntry entry{};

        Entry() {
            extended_entries = nullptr;
        }

        ~Entry() {
            clear();
        }

        LongFileNameEntry *top() const {
            if (extended_entries != nullptr) {
                return extended_entries->entry;
            }
            return nullptr;
        }

        unsigned int size() const {
            return num_entries;
        }

        /**
          * \brief Check if the stack is empty.
          */
        bool empty() const {
            if (size() == 0) {
                return true;
            }
            return false;
        }

        /**
          * \brief Push a new entry into the extended_entries stack.
          *
          * \param new_entry The new extended entry to push.
          * \param force (optional) Force the insertion of a new entry even if it is out of order.
          *
          * \return Success when successfully inserted,
          * Error when cannot insert.
          */
        ExtendedArrayError push(const LongFileNameEntry new_entry, const bool force = false) {
            if (empty()) {
                extended_entries = create_new(new_entry);
                num_entries++;
                return ExtendedArrayError::Success;
            }

            if (new_entry < *top() || force) {
                auto cur = create_new(new_entry);
                cur->next = extended_entries;
                extended_entries = cur;
                num_entries++;
                return ExtendedArrayError::Success;
            }

            return ExtendedArrayError::BadPosition;
        }

        /**
          * \brief Remove the top entry in the stack.
          *
          * \return Success when successfully removed,
          * Error when cannot remove.
          */
        ExtendedArrayError pop() {
            if (empty()) {
                return ExtendedArrayError::Empty;
            }

            if (extended_entries == nullptr) {
                return ExtendedArrayError::InvalidEntry;
            }

            replace_head();

            return ExtendedArrayError::Success;
        }

        /**
          * \brief Remove all entries in the stack.
          */
        void clear() {
            while (extended_entries != nullptr) {
                replace_head();
            }
        }

        /**
          * \brief Get the filename from the stack, or get a fundamental name.
          *
          * After acquiring the name from the stack, the stack is cleared.
          *
          * \return An independent C string.
          */
        const char *get_filename();
    };

    // These functions all required return value to be little-endian.
    typedef std::uint32_t (*ImageReadFunc)(void *userdata, void *buffer, std::uint32_t bytes);
    typedef std::uint32_t (*ImageSeekFunc)(void *userdata, std::uint32_t offset, int mode);

    enum ImageSeekMode {
        IMAGE_SEEK_MODE_BEG,
        IMAGE_SEEK_MODE_CUR,
        IMAGE_SEEK_MODE_END
    };

    struct Image {
        BootBlock boot_block;
        ImageReadFunc read_func;
        ImageSeekFunc seek_func;
        void *userdata;

        explicit Image(void *userdata, ImageReadFunc read_func, ImageSeekFunc seek_func);

        /**
         * \brief Get the next entry to given entry.
         *
         * \returns True if the number is valid and the get performs success.
         */
        bool get_next_entry(Entry &entry);

        /**
         * \brief       Get first entry of a directory.
         * \returns     True on success.
         */
        bool get_first_entry_dir(Entry &parent, Entry &first);

        /**
         * \brief Get total of bytes a cluster consists of.
         */
        std::uint32_t bytes_per_cluster() const;

        /**
         * \brief   Reading data from the FAT image, starting at given cluster.
         *
         * \param   dest_buffer       The buffer contains read result.
         * \param   starting_cluster  The first cluster that contains the data.
         * \param   size              The size of data to read.
         *
         * \returns Number of bytes read.
         */
        std::uint32_t read_from_cluster(std::uint8_t *dest_buffer, const std::uint32_t offset,
            const ClusterID starting_cluster, const std::uint32_t size);

        /**
         * \brief Get the read cursor of current image.
         * \internal
         */
        std::uint32_t get_current_image_offset();

        /**
         * \brief   Get the successor cluster ID of the given cluster.
         * \param   target The ID of the cluster.
         * \returns Successor cluster ID.
         */
        ClusterID get_successor_cluster(const ClusterID target);
    };

    static_assert(sizeof(BootBlock) == 512, "Boot block size doesn't match to what expected.");
    static_assert(sizeof(FundamentalEntry) == 32, "Fundamental entry size doesn't match to what expected.");
    static_assert(sizeof(LongFileNameEntry) == 32, "LFN entry size doesn't match to what expected.");
} // namespace Fat16
