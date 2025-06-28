#ifndef SIGHTREAD_DETAIL_QBMIDI_HPP
#define SIGHTREAD_DETAIL_QBMIDI_HPP

#include <any>
#include <cstdint>
#include <span>
#include <vector>

namespace SightRead::Detail {
struct QbHeader {
    std::uint32_t flags;
    std::uint32_t file_size;
};

enum class QbItemType : std::uint8_t {
    StructFlag = 0,
    Integer = 1,
    Float = 2,
    String = 3,
    WideString = 4,
    Struct = 10,
    Array = 12,
    QbKey = 13,
    Pointer = 26
};

struct QbItemInfo {
    std::uint8_t flags;
    QbItemType type;
};

struct QbSharedProps {
    std::uint32_t id;
    std::uint32_t qb_name;
    std::any value;
};

struct QbItem {
    QbItemInfo info;
    QbSharedProps props;
    std::any data;
};

struct QbMidi {
    QbHeader header;
    std::vector<QbItem> items;
};

struct QbStructInfo {
    std::uint8_t flags;
    QbItemType type;
};

struct QbStructProps {
    std::uint32_t id;
    std::any value;
    std::uint32_t next_item;
};

struct QbStructItem {
    QbStructInfo info;
    QbStructProps props;
    std::any data;
};

struct QbStructData {
    std::uint32_t header_marker;
    std::uint32_t item_offset;
    std::vector<QbStructItem> items;
};

enum class Endianness { LittleEndian, BigEndian };

QbMidi parse_qb(std::span<const std::uint8_t> data, Endianness endianness);
}

#endif
