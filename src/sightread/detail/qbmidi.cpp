#include <unordered_map>

#include "sightread/detail/utils.hpp"
#include "sightread/songparts.hpp"

#include "qbmidi.hpp"

namespace {
class QbReader {
private:
    SightRead::Detail::Endianness m_endianness;
    std::span<const std::uint8_t> m_full_file;
    std::span<const std::uint8_t> m_remaining_data;

    void advance_bytes(std::size_t byte_count)
    {
        m_remaining_data = m_remaining_data.subspan(byte_count);
    }

    void snap_to_four_byte_alignment()
    {
        const auto bytes_read = m_full_file.size() - m_remaining_data.size();
        advance_bytes((~bytes_read + 1) & 0b11);
    }

    void move_to_position(std::size_t position)
    {
        m_remaining_data = m_full_file.subspan(position);
    }

    float read_float()
    {
        float value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<float>(m_remaining_data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<float>(m_remaining_data, 0);
            break;
        }
        advance_bytes(4);
        return value;
    }

    std::int32_t read_int32()
    {
        std::int32_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<std::int32_t>(m_remaining_data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<std::int32_t>(m_remaining_data, 0);
            break;
        }
        advance_bytes(4);
        return value;
    }

    std::uint32_t read_le_uint32()
    {
        const auto value
            = read_four_byte_le<std::uint32_t>(m_remaining_data, 0);
        advance_bytes(4);
        return value;
    }

    std::uint32_t read_uint32()
    {
        std::uint32_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<std::uint32_t>(m_remaining_data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<std::uint32_t>(m_remaining_data, 0);
            break;
        }
        advance_bytes(4);
        return value;
    }

    std::uint16_t read_uint16()
    {
        std::uint16_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_two_byte_be<std::uint16_t>(m_remaining_data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_two_byte_le<std::uint16_t>(m_remaining_data, 0);
            break;
        }
        advance_bytes(2);
        return value;
    }

    char read_char() { return static_cast<char>(pop_front(m_remaining_data)); }

    [[nodiscard]] SightRead::Detail::QbItemType
    qb_item_type(std::uint8_t byte) const
    {
        const std::unordered_map<int, SightRead::Detail::QbItemType>
            qb_type_mapping {{0, SightRead::Detail::QbItemType::StructFlag},
                             {1, SightRead::Detail::QbItemType::Integer},
                             {2, SightRead::Detail::QbItemType::Float},
                             {3, SightRead::Detail::QbItemType::String},
                             {4, SightRead::Detail::QbItemType::WideString},
                             {10, SightRead::Detail::QbItemType::Struct},
                             {12, SightRead::Detail::QbItemType::Array},
                             {13, SightRead::Detail::QbItemType::QbKey},
                             {26, SightRead::Detail::QbItemType::Pointer}};

        return qb_type_mapping.at(byte);
    }

    [[nodiscard]] SightRead::Detail::QbItemType
    struct_item_type(std::uint8_t byte) const
    {
        if (m_endianness == SightRead::Detail::Endianness::BigEndian) {
            return qb_item_type(byte);
        }

        const std::unordered_map<int, SightRead::Detail::QbItemType>
            ps2_qb_mapping {{3, SightRead::Detail::QbItemType::Integer},
                            {5, SightRead::Detail::QbItemType::Float},
                            {7, SightRead::Detail::QbItemType::String},
                            {21, SightRead::Detail::QbItemType::Struct},
                            {27, SightRead::Detail::QbItemType::QbKey},
                            {53, SightRead::Detail::QbItemType::Pointer}};

        return ps2_qb_mapping.at(byte);
    }

    std::vector<std::any> read_array_node()
    {
        const auto first_item = read_item_info();
        const auto item_count = read_uint32();
        std::vector<std::any> array;
        array.reserve(item_count);

        switch (first_item.type) {
        case SightRead::Detail::QbItemType::StructFlag:
            advance_bytes(4);
            break;
        case SightRead::Detail::QbItemType::Integer: {
            if (item_count > 1) {
                const auto list_start = read_uint32();
                move_to_position(list_start);
            }
            for (auto i = 0U; i < item_count; ++i) {
                array.emplace_back(read_uint32());
            }
            break;
        }
        case SightRead::Detail::QbItemType::Struct: {
            const auto list_start = read_uint32();
            move_to_position(list_start);
            std::vector<std::uint32_t> start_list;
            if (item_count == 1) {
                start_list.push_back(list_start);
            } else {
                for (auto i = 0U; i < item_count; ++i) {
                    start_list.push_back(read_uint32());
                }
            }

            for (auto start : start_list) {
                move_to_position(start);
                array.emplace_back(read_struct_data());
            }

            break;
        }
        case SightRead::Detail::QbItemType::Array: {
            const auto list_start = read_uint32();
            move_to_position(list_start);
            std::vector<std::uint32_t> start_list;
            if (item_count == 1) {
                start_list.push_back(list_start);
            } else {
                for (auto i = 0U; i < item_count; ++i) {
                    start_list.push_back(read_uint32());
                }
            }

            for (auto start : start_list) {
                move_to_position(start);
                array.emplace_back(read_array_node());
            }

            break;
        }
        case SightRead::Detail::QbItemType::Float:
        case SightRead::Detail::QbItemType::Pointer:
        case SightRead::Detail::QbItemType::QbKey:
        case SightRead::Detail::QbItemType::WideString:
        default:
            throw SightRead::ParseError(
                "Unexpected type for array element, "
                + std::to_string(static_cast<int>(first_item.type)));
        }

        return array;
    }

    SightRead::Detail::QbHeader read_header()
    {
        constexpr auto REST_OF_HEADER_SIZE = 20;

        const auto flags = read_uint32();
        const auto file_size = read_uint32();
        advance_bytes(REST_OF_HEADER_SIZE);
        return {flags, file_size};
    }

    SightRead::Detail::QbItem read_item()
    {
        const auto info = read_item_info();
        const auto shared_props = read_shared_props(info.type);
        const auto data_value = read_value(info.type, shared_props.value);
        return {info, shared_props, data_value};
    }

    SightRead::Detail::QbItemInfo read_item_info()
    {
        const auto info = read_le_uint32();
        const auto flags = static_cast<std::uint8_t>(info >> 8);
        const auto type = qb_item_type((info >> 16) & 0x7F);
        return {flags, type};
    }

    SightRead::Detail::QbSharedProps
    read_shared_props(SightRead::Detail::QbItemType type)
    {
        const auto id = read_uint32();
        const auto qb_name = read_uint32();

        SightRead::Detail::QbSharedProps props {id, qb_name, {}};
        switch (type) {
        case SightRead::Detail::QbItemType::Array:
            props.value = read_uint32();
            break;
        case SightRead::Detail::QbItemType::Float:
        case SightRead::Detail::QbItemType::Integer:
        case SightRead::Detail::QbItemType::Pointer:
        case SightRead::Detail::QbItemType::QbKey:
        case SightRead::Detail::QbItemType::Struct:
        case SightRead::Detail::QbItemType::StructFlag:
        case SightRead::Detail::QbItemType::WideString:
        default:
            throw SightRead::ParseError(
                "Unexpected type for QbSharedProps, "
                + std::to_string(static_cast<int>(type)));
        }

        advance_bytes(4);
        return props;
    }

    std::any read_simple_value(SightRead::Detail::QbItemType type)
    {
        switch (type) {
        case SightRead::Detail::QbItemType::Integer:
            return read_int32();
        case SightRead::Detail::QbItemType::Float:
            return read_float();
        case SightRead::Detail::QbItemType::Pointer:
        case SightRead::Detail::QbItemType::QbKey:
        case SightRead::Detail::QbItemType::String:
        case SightRead::Detail::QbItemType::Struct:
        case SightRead::Detail::QbItemType::WideString:
            return read_uint32();
        case SightRead::Detail::QbItemType::Array:
        case SightRead::Detail::QbItemType::StructFlag:
        default:
            throw SightRead::ParseError(
                "Need to read simple "
                + std::to_string(static_cast<int>(type)));
        }
    }

    std::string read_string()
    {
        std::string value;

        while (true) {
            const auto character = read_char();
            if (character == 0) {
                return value;
            }
            value.push_back(character);
        }
    }

    SightRead::Detail::QbStructData read_struct_data()
    {
        const auto header_marker = read_uint32();
        const auto item_offset = read_uint32();
        std::vector<SightRead::Detail::QbStructItem> items;
        auto next_item = item_offset;

        while (next_item != 0) {
            move_to_position(next_item);
            items.push_back(read_struct_item());
            next_item = items.back().props.next_item;
        }

        return {header_marker, item_offset, std::move(items)};
    }

    SightRead::Detail::QbStructInfo read_struct_info()
    {
        const auto info = read_le_uint32();
        const auto flags = static_cast<std::uint8_t>(info >> 8);
        auto info_byte = flags;
        const auto info_byte_2 = static_cast<std::uint8_t>(info >> 16);
        if (info_byte == 1 && info_byte_2 != 0) {
            info_byte = info_byte_2;
        }
        const auto type = struct_item_type(info_byte & 0x7F);
        return {flags, type};
    }

    SightRead::Detail::QbStructItem read_struct_item()
    {
        const auto info = read_struct_info();
        const auto props = read_struct_props(info.type);
        const auto data_value = read_value(info.type, props.value);

        return {info, props, data_value};
    }

    SightRead::Detail::QbStructProps
    read_struct_props(SightRead::Detail::QbItemType type)
    {
        const auto id = read_uint32();
        const auto value = read_simple_value(type);
        const auto next_item = read_uint32();
        return {id, value, next_item};
    }

    std::any read_value(SightRead::Detail::QbItemType type,
                        const std::any& default_value)
    {
        std::any value;

        switch (type) {
        case SightRead::Detail::QbItemType::Float:
        case SightRead::Detail::QbItemType::Integer:
        case SightRead::Detail::QbItemType::Pointer:
        case SightRead::Detail::QbItemType::QbKey:
            value = default_value;
            break;
        case SightRead::Detail::QbItemType::Array:
            value = read_array_node();
            break;
        case SightRead::Detail::QbItemType::String:
            value = read_string();
            break;
        case SightRead::Detail::QbItemType::Struct:
            value = read_struct_data();
            break;
        case SightRead::Detail::QbItemType::WideString:
            value = read_widestring();
            break;
        case SightRead::Detail::QbItemType::StructFlag:
        default:
            throw SightRead::ParseError(
                "Unexpected type for Qb value, "
                + std::to_string(static_cast<int>(type)));
        }

        snap_to_four_byte_alignment();

        return value;
    }

    std::wstring read_widestring()
    {
        std::wstring value;

        while (true) {
            const auto character = read_uint16();
            if (character == 0) {
                return value;
            }
            value.push_back(character);
        }
    }

public:
    QbReader(std::span<const std::uint8_t> file,
             SightRead::Detail::Endianness endianness)
        : m_endianness {endianness}
        , m_full_file {file}
        , m_remaining_data {file}
    {
    }

    SightRead::Detail::QbMidi parse()
    {
        const auto header = read_header();
        std::vector<SightRead::Detail::QbItem> items;

        while (!m_remaining_data.empty()) {
            items.push_back(read_item());
        }

        return {header, items};
    }
};
}

SightRead::Detail::QbMidi
SightRead::Detail::parse_qb(std::span<const std::uint8_t> data,
                            Endianness endianness)
{
    QbReader reader {data, endianness};
    return reader.parse();
}
