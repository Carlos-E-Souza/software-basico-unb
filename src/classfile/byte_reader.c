/**
 * @file byte_reader.c
 * @brief Implementação do leitor sequencial de bytes em big-endian.
 */
#include "classfile/byte_reader.h"

void br_init(ByteReader *reader, const uint8_t *data, size_t size)
{
    reader->data = data;
    reader->size = size;
    reader->pos = 0;
    reader->base = 0;
}

size_t br_remaining(const ByteReader *reader)
{
    return reader->size - reader->pos;
}

size_t br_offset(const ByteReader *reader)
{
    return reader->base + reader->pos;
}

bool br_read_u1(ByteReader *reader, uint8_t *out)
{
    if (br_remaining(reader) < 1) {
        return false;
    }
    *out = reader->data[reader->pos];
    reader->pos += 1;
    return true;
}

bool br_read_u2(ByteReader *reader, uint16_t *out)
{
    const uint8_t *p;
    if (br_remaining(reader) < 2) {
        return false;
    }
    p = reader->data + reader->pos;
    *out = (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
    reader->pos += 2;
    return true;
}

bool br_read_u4(ByteReader *reader, uint32_t *out)
{
    const uint8_t *p;
    if (br_remaining(reader) < 4) {
        return false;
    }
    p = reader->data + reader->pos;
    *out = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    reader->pos += 4;
    return true;
}

bool br_read_bytes(ByteReader *reader, size_t count, const uint8_t **out)
{
    if (br_remaining(reader) < count) {
        return false;
    }
    *out = reader->data + reader->pos;
    reader->pos += count;
    return true;
}

bool br_sub_reader(ByteReader *reader, size_t count, ByteReader *sub)
{
    if (br_remaining(reader) < count) {
        return false;
    }
    sub->data = reader->data + reader->pos;
    sub->size = count;
    sub->pos = 0;
    sub->base = br_offset(reader);
    reader->pos += count;
    return true;
}
