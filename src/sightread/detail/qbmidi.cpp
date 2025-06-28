#include "sightread/detail/utils.hpp"
#include "sightread/songparts.hpp"

#include "qbmidi.hpp"

namespace {
class QbReader {
private:
    SightRead::Detail::Endianness m_endianness;

    float read_float(std::span<const std::uint8_t>& data) const
    {
        float value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<float>(data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<float>(data, 0);
            break;
        }
        data = data.subspan(4);
        return value;
    }

    std::int32_t read_int32(std::span<const std::uint8_t>& data) const
    {
        std::int32_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<std::int32_t>(data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<std::int32_t>(data, 0);
            break;
        }
        data = data.subspan(4);
        return value;
    }

    std::uint32_t read_uint32(std::span<const std::uint8_t>& data) const
    {
        std::uint32_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_four_byte_be<std::uint32_t>(data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_four_byte_le<std::uint32_t>(data, 0);
            break;
        }
        data = data.subspan(4);
        return value;
    }

    std::uint16_t read_uint16(std::span<const std::uint8_t>& data) const
    {
        std::uint16_t value = 0;
        switch (m_endianness) {
        case SightRead::Detail::Endianness::BigEndian:
            value = read_two_byte_be<std::uint16_t>(data, 0);
            break;
        case SightRead::Detail::Endianness::LittleEndian:
            value = read_two_byte_le<std::uint16_t>(data, 0);
            break;
        }
        data = data.subspan(2);
        return value;
    }

    SightRead::Detail::QbItemType struct_item_type(std::uint8_t byte) const
    {
        if (m_endianness == SightRead::Detail::Endianness::BigEndian) {
            return static_cast<SightRead::Detail::QbItemType>(byte);
        }

        switch (byte) {
        case 3:
            return SightRead::Detail::QbItemType::Integer;
        case 5:
            return SightRead::Detail::QbItemType::Float;
        case 7:
            return SightRead::Detail::QbItemType::String;
        case 21:
            return SightRead::Detail::QbItemType::Struct;
        case 27:
            return SightRead::Detail::QbItemType::QbKey;
        case 53:
            return SightRead::Detail::QbItemType::Pointer;
        default:
            throw SightRead::ParseError("Unknown PS2 item type "
                                        + std::to_string(byte));
        }
    }

public:
    QbReader(SightRead::Detail::Endianness endianness)
        : m_endianness {endianness}
    {
    }

    std::vector<std::any> read_array_node(std::span<const std::uint8_t>& data,
                                          std::uint32_t file_size) const
    {
        const auto first_item = read_item_info(data);
        const auto item_count = read_uint32(data);
        std::vector<std::any> array;
        array.reserve(item_count);

        switch (first_item.type) {
        case SightRead::Detail::QbItemType::StructFlag:
            data = data.subspan(4);
            break;
        case SightRead::Detail::QbItemType::Integer: {
            if (item_count > 1) {
                const auto list_start = read_uint32(data);
                data = data.subspan(list_start + data.size() - file_size);
            }
            for (auto i = 0U; i < item_count; ++i) {
                array.emplace_back(read_uint32(data));
            }
            break;
        }
        case SightRead::Detail::QbItemType::Struct: {
            const auto list_start = read_uint32(data);
            data = data.subspan(list_start + data.size() - file_size);
            std::vector<std::uint32_t> start_list;
            if (item_count == 1) {
                start_list.push_back(list_start);
            } else {
                for (auto i = 0U; i < item_count; ++i) {
                    start_list.push_back(read_uint32(data));
                }
            }

            for (auto start : start_list) {
                data = data.subspan(data.size() - file_size + start);
                array.emplace_back(read_struct_data(data, file_size));
            }

            break;
        }
        case SightRead::Detail::QbItemType::Array: {
            const auto list_start = read_uint32(data);
            data = data.subspan(list_start + data.size() - file_size);
            std::vector<std::uint32_t> start_list;
            if (item_count == 1) {
                start_list.push_back(list_start);
            } else {
                for (auto i = 0U; i < item_count; ++i) {
                    start_list.push_back(read_uint32(data));
                }
            }

            for (auto start : start_list) {
                data = data.subspan(data.size() - file_size + start);
                array.emplace_back(read_array_node(data, file_size));
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

    SightRead::Detail::QbHeader
    read_header(std::span<const std::uint8_t>& data) const
    {
        constexpr auto REST_OF_HEADER_SIZE = 20;

        const auto flags = read_uint32(data);
        const auto file_size = read_uint32(data);
        data = data.subspan(REST_OF_HEADER_SIZE);
        return {flags, file_size};
    }

    SightRead::Detail::QbItem read_item(std::span<const std::uint8_t>& data,
                                        std::uint32_t file_size) const
    {
        const auto info = read_item_info(data);
        const auto shared_props = read_shared_props(data, info.type);
        const auto data_value
            = read_value(data, file_size, info.type, shared_props.value);
        return {info, shared_props, data_value};
    }

    SightRead::Detail::QbItemInfo
    read_item_info(std::span<const std::uint8_t>& data) const
    {
        const auto info = read_four_byte_le<std::uint32_t>(data, 0);
        const auto flags = static_cast<std::uint8_t>(info >> 8);
        const auto type
            = static_cast<SightRead::Detail::QbItemType>(info >> 16);
        data = data.subspan(4);
        return {flags, type};
    }

    SightRead::Detail::QbSharedProps
    read_shared_props(std::span<const std::uint8_t>& data,
                      SightRead::Detail::QbItemType type) const
    {
        const auto id = read_uint32(data);
        const auto qb_name = read_uint32(data);

        SightRead::Detail::QbSharedProps props {id, qb_name, {}};
        switch (type) {
        case SightRead::Detail::QbItemType::Array:
            props.value = read_uint32(data);
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

        data = data.subspan(4);
        return props;
    }

    std::any read_simple_value(std::span<const std::uint8_t>& data,
                               SightRead::Detail::QbItemType type) const
    {
        switch (type) {
        case SightRead::Detail::QbItemType::Integer:
            return read_int32(data);
        case SightRead::Detail::QbItemType::Float:
            return read_float(data);
        case SightRead::Detail::QbItemType::Pointer:
        case SightRead::Detail::QbItemType::QbKey:
        case SightRead::Detail::QbItemType::String:
        case SightRead::Detail::QbItemType::Struct:
        case SightRead::Detail::QbItemType::WideString:
            return read_uint32(data);
        case SightRead::Detail::QbItemType::Array:
        case SightRead::Detail::QbItemType::StructFlag:
        default:
            throw SightRead::ParseError(
                "Need to read simple "
                + std::to_string(static_cast<int>(type)));
        }
    }

    std::string read_string(std::span<const std::uint8_t>& data) const
    {
        std::string value;

        while (true) {
            const auto character = pop_front(data);
            if (character == 0) {
                return value;
            }
            value.push_back(character);
        }
    }

    SightRead::Detail::QbStructData
    read_struct_data(std::span<const std::uint8_t>& data,
                     std::uint32_t file_size) const
    {
        const auto header_marker = read_uint32(data);
        const auto item_offset = read_uint32(data);
        std::vector<SightRead::Detail::QbStructItem> items;
        auto next_item = item_offset;

        while (next_item != 0) {
            data = data.subspan(next_item + data.size() - file_size);
            items.push_back(read_struct_item(data, file_size));
            next_item = items.back().props.next_item;
        }

        return {header_marker, item_offset, std::move(items)};
    }

    SightRead::Detail::QbStructInfo
    read_struct_info(std::span<const std::uint8_t>& data) const
    {
        const auto info = read_four_byte_le<std::uint32_t>(data, 0);
        const auto flags = static_cast<std::uint8_t>(info >> 8);
        auto info_byte = flags;
        const auto info_byte_2 = static_cast<std::uint8_t>(info >> 16);
        if (info_byte == 1 && info_byte_2 != 0) {
            info_byte = info_byte_2;
        }
        const auto type = struct_item_type(info_byte & 0x7F);
        data = data.subspan(4);
        return {flags, type};
    }

    SightRead::Detail::QbStructItem
    read_struct_item(std::span<const std::uint8_t>& data,
                     std::uint32_t file_size) const
    {
        const auto info = read_struct_info(data);
        const auto props = read_struct_props(data, info.type);
        const auto data_value
            = read_value(data, file_size, info.type, props.value);

        return {info, props, data_value};
    }

    SightRead::Detail::QbStructProps
    read_struct_props(std::span<const std::uint8_t>& data,
                      SightRead::Detail::QbItemType type) const
    {
        const auto id = read_uint32(data);
        const auto value = read_simple_value(data, type);
        const auto next_item = read_uint32(data);
        return {id, value, next_item};
    }

    std::any read_value(std::span<const std::uint8_t>& data,
                        std::uint32_t file_size,
                        SightRead::Detail::QbItemType type,
                        const std::any& default_value) const
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
            value = read_array_node(data, file_size);
            break;
        case SightRead::Detail::QbItemType::String:
            value = read_string(data);
            break;
        case SightRead::Detail::QbItemType::Struct:
            value = read_struct_data(data, file_size);
            break;
        case SightRead::Detail::QbItemType::WideString:
            value = read_widestring(data);
            break;
        case SightRead::Detail::QbItemType::StructFlag:
        default:
            throw SightRead::ParseError(
                "Unexpected type for Qb value, "
                + std::to_string(static_cast<int>(type)));
        }

        data = data.subspan((data.size() - file_size) % 4);

        return value;
    }

    std::wstring read_widestring(std::span<const std::uint8_t>& data) const
    {
        std::wstring value;

        while (true) {
            const auto character = read_uint16(data);
            if (character == 0) {
                return value;
            }
            value.push_back(character);
        }
    }
};
}

SightRead::Detail::QbMidi
SightRead::Detail::parse_qb(std::span<const std::uint8_t> data,
                            Endianness endianness)
{
    const auto reader = QbReader(endianness);
    const auto header = reader.read_header(data);
    std::vector<SightRead::Detail::QbItem> items;

    while (!data.empty()) {
        items.push_back(reader.read_item(data, header.file_size));
    }

    return {header, items};
}
