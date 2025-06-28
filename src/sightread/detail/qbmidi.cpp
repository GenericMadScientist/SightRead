#include "sightread/detail/utils.hpp"
#include "sightread/songparts.hpp"

#include "qbmidi.hpp"

namespace {
SightRead::Detail::QbHeader read_qb_header(std::span<const std::uint8_t>& data)
{
    constexpr auto HEADER_SIZE = 28;

    const auto flags = read_four_byte_be<std::uint32_t>(data, 0);
    const auto file_size = read_four_byte_be<std::uint32_t>(data, 4);
    data = data.subspan(HEADER_SIZE);
    return {flags, file_size};
}

SightRead::Detail::QbItemInfo
read_qb_item_info(std::span<const std::uint8_t>& data)
{
    const auto info = read_four_byte_le<std::uint32_t>(data, 0);
    const auto flags = static_cast<std::uint8_t>(info >> 8);
    const auto type = static_cast<SightRead::Detail::QbItemType>(info >> 16);
    data = data.subspan(4);
    return {flags, type};
}

SightRead::Detail::QbSharedProps
read_qb_shared_props(std::span<const std::uint8_t>& data,
                     SightRead::Detail::QbItemType type)
{
    constexpr auto PROPS_SIZE = 16;
    constexpr auto PROPS_VALUE_OFFSET = 8;

    const auto id = read_four_byte_be<std::uint32_t>(data, 0);
    const auto qb_name = read_four_byte_be<std::uint32_t>(data, 4);

    SightRead::Detail::QbSharedProps props {id, qb_name, {}};
    switch (type) {
    case SightRead::Detail::QbItemType::Array:
        props.value
            = read_four_byte_be<std::uint32_t>(data, PROPS_VALUE_OFFSET);
        break;
    case SightRead::Detail::QbItemType::Float:
    case SightRead::Detail::QbItemType::Integer:
    case SightRead::Detail::QbItemType::Pointer:
    case SightRead::Detail::QbItemType::QbKey:
    case SightRead::Detail::QbItemType::Struct:
    case SightRead::Detail::QbItemType::StructFlag:
    case SightRead::Detail::QbItemType::WideString:
    default:
        throw SightRead::ParseError("Unexpected type for QbSharedProps, "
                                    + std::to_string(static_cast<int>(type)));
    }

    data = data.subspan(PROPS_SIZE);
    return props;
}

SightRead::Detail::QbStructInfo
read_qb_struct_info(std::span<const std::uint8_t>& data)
{
    const auto info = read_four_byte_le<std::uint32_t>(data, 0);
    const auto flags = static_cast<std::uint8_t>(info >> 8);
    auto info_byte = flags;
    const auto info_byte_2 = static_cast<std::uint8_t>(info >> 16);
    if (info_byte == 1 && info_byte_2 != 0) {
        info_byte = info_byte_2;
    }
    const auto type
        = static_cast<SightRead::Detail::QbItemType>(info_byte & 0x7F);
    data = data.subspan(4);
    return {flags, type};
}

std::any read_qb_simple_value(std::span<const std::uint8_t>& data,
                              SightRead::Detail::QbItemType type)
{
    switch (type) {
    case SightRead::Detail::QbItemType::Integer: {
        const auto value = read_four_byte_be<std::int32_t>(data, 0);
        data = data.subspan(4);
        return value;
    }
    case SightRead::Detail::QbItemType::Float: {
        const auto value = read_four_byte_be<float>(data, 0);
        data = data.subspan(4);
        return value;
    }
    case SightRead::Detail::QbItemType::Pointer:
    case SightRead::Detail::QbItemType::QbKey:
    case SightRead::Detail::QbItemType::Struct:
    case SightRead::Detail::QbItemType::WideString: {
        const auto value = read_four_byte_be<std::uint32_t>(data, 0);
        data = data.subspan(4);
        return value;
    }
    case SightRead::Detail::QbItemType::Array:
    case SightRead::Detail::QbItemType::StructFlag:
    default:
        throw SightRead::ParseError("Need to read simple "
                                    + std::to_string(static_cast<int>(type)));
    }
}

SightRead::Detail::QbStructProps
read_qb_struct_props(std::span<const std::uint8_t>& data,
                     SightRead::Detail::QbItemType type)
{
    const auto id = read_four_byte_be<std::uint32_t>(data, 0);
    data = data.subspan(4);
    const auto value = read_qb_simple_value(data, type);
    const auto next_item = read_four_byte_be<std::uint32_t>(data, 0);
    data = data.subspan(4);
    return {id, value, next_item};
}

std::any read_qb_value(std::span<const std::uint8_t>& data,
                       std::uint32_t file_size,
                       SightRead::Detail::QbItemType type,
                       const std::any& default_value);

SightRead::Detail::QbStructItem
read_qb_struct_item(std::span<const std::uint8_t>& data,
                    std::uint32_t file_size)
{
    const auto info = read_qb_struct_info(data);
    const auto props = read_qb_struct_props(data, info.type);
    const auto data_value
        = read_qb_value(data, file_size, info.type, props.value);

    return {info, props, data_value};
}

SightRead::Detail::QbStructData
read_qb_struct_data(std::span<const std::uint8_t>& data,
                    std::uint32_t file_size)
{
    const auto header_marker = read_four_byte_be<std::uint32_t>(data, 0);
    const auto item_offset = read_four_byte_be<std::uint32_t>(data, 4);
    std::vector<SightRead::Detail::QbStructItem> items;
    auto next_item = item_offset;

    while (next_item != 0) {
        data = data.subspan(next_item + data.size() - file_size);
        items.push_back(read_qb_struct_item(data, file_size));
        next_item = items.back().props.next_item;
    }

    return {header_marker, item_offset, std::move(items)};
}

std::vector<std::any> read_qb_array_node(std::span<const std::uint8_t>& data,
                                         std::uint32_t file_size)
{
    const auto first_item = read_qb_item_info(data);
    const auto item_count = read_four_byte_be<std::uint32_t>(data, 0);
    const auto array_byte_size = 4 * item_count;
    data = data.subspan(4);
    std::vector<std::any> array;
    array.reserve(item_count);

    switch (first_item.type) {
    case SightRead::Detail::QbItemType::StructFlag:
        data = data.subspan(4);
        break;
    case SightRead::Detail::QbItemType::Integer: {
        if (item_count > 1) {
            const auto list_start = read_four_byte_be<std::uint32_t>(data, 0);
            data = data.subspan(list_start + data.size() - file_size);
        }
        for (auto i = 0U; i < array_byte_size; i += 4) {
            array.emplace_back(read_four_byte_be<std::int32_t>(data, i));
        }
        data = data.subspan(array_byte_size);
        break;
    }
    case SightRead::Detail::QbItemType::Struct: {
        const auto list_start = read_four_byte_be<std::uint32_t>(data, 0);
        data = data.subspan(list_start + data.size() - file_size);
        std::vector<std::uint32_t> start_list;
        if (item_count == 1) {
            start_list.push_back(list_start);
        } else {
            for (auto i = 0U; i < array_byte_size; i += 4) {
                start_list.push_back(read_four_byte_be<std::uint32_t>(data, i));
            }
            data = data.subspan(array_byte_size);
        }

        for (auto start : start_list) {
            data = data.subspan(data.size() - file_size + start);
            array.emplace_back(read_qb_struct_data(data, file_size));
        }

        break;
    }
    case SightRead::Detail::QbItemType::Array: {
        const auto list_start = read_four_byte_be<std::uint32_t>(data, 0);
        data = data.subspan(list_start + data.size() - file_size);
        std::vector<std::uint32_t> start_list;
        if (item_count == 1) {
            start_list.push_back(list_start);
        } else {
            for (auto i = 0U; i < array_byte_size; i += 4) {
                start_list.push_back(read_four_byte_be<std::uint32_t>(data, i));
            }
            data = data.subspan(array_byte_size);
        }

        for (auto start : start_list) {
            data = data.subspan(data.size() - file_size + start);
            array.emplace_back(read_qb_array_node(data, file_size));
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

std::wstring read_qb_widestring(std::span<const std::uint8_t>& data)
{
    std::wstring value;

    while (true) {
        const auto character = read_two_byte_le<std::uint16_t>(data, 0);
        data = data.subspan(2);
        if (character == 0) {
            return value;
        }
        value.push_back(character);
    }
}

std::any read_qb_value(std::span<const std::uint8_t>& data,
                       std::uint32_t file_size,
                       SightRead::Detail::QbItemType type,
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
    case SightRead::Detail::QbItemType::Struct:
        value = read_qb_struct_data(data, file_size);
        break;
    case SightRead::Detail::QbItemType::Array:
        value = read_qb_array_node(data, file_size);
        break;
    case SightRead::Detail::QbItemType::WideString:
        value = read_qb_widestring(data);
        break;
    case SightRead::Detail::QbItemType::StructFlag:
    default:
        throw SightRead::ParseError("Unexpected type for Qb value, "
                                    + std::to_string(static_cast<int>(type)));
    }

    data = data.subspan((data.size() - file_size) % 4);

    return value;
}

SightRead::Detail::QbItem read_qb_item(std::span<const std::uint8_t>& data,
                                       std::uint32_t file_size)
{
    const auto info = read_qb_item_info(data);
    const auto shared_props = read_qb_shared_props(data, info.type);
    const auto data_value
        = read_qb_value(data, file_size, info.type, shared_props.value);
    return {info, shared_props, data_value};
}
}

SightRead::Detail::QbMidi
SightRead::Detail::parse_qb(std::span<const std::uint8_t> data)
{
    const auto header = read_qb_header(data);
    std::vector<SightRead::Detail::QbItem> items;

    while (!data.empty()) {
        items.push_back(read_qb_item(data, header.file_size));
    }

    return {header, items};
}
