#pragma once

#include <cstdint>

class GlBuffer {
public:
    GlBuffer(uint64_t size, uint32_t usage = 0, const void *data = nullptr);
    ~GlBuffer();

    uint32_t id() const { return gl_buffer_; }

    uint64_t size() const { return size_; }

    void *map(bool write = false);
    template <typename T>
    T *typed_map(bool write = false) { return reinterpret_cast<T *>(map(write)); }
    void unmap();

private:
    uint32_t gl_buffer_ = 0;
    uint64_t size_;
    void *mapped_ptr_ = nullptr;
};

class GlTexture2D {
public:
    GlTexture2D(uint32_t format, uint32_t width, uint32_t height, uint32_t levels = 0);
    ~GlTexture2D();

    uint32_t id() const { return gl_texture_; }

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t levels() const { return levels_; }
    uint32_t format() const { return format_; }

    void set_data(const void *data, uint32_t level = 0);

    void generate_mipmap();

private:
    uint32_t gl_texture_ = 0;
    uint32_t width_;
    uint32_t height_;
    uint32_t levels_;
    uint32_t format_;
    uint32_t channel_format_;
    uint32_t channel_type_;
};
