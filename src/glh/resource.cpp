#include "resource.hpp"

#include <cmath>

#include <glad/glad.h>

namespace {

void get_channel_format_type(uint32_t format, uint32_t &channel_format, uint32_t &channel_type) {
    if (format == GL_RGB8) {
        channel_format = GL_RGB;
        channel_type = GL_UNSIGNED_BYTE;
    } else if (format == GL_SRGB8) {
        channel_format = GL_SRGB;
        channel_type = GL_UNSIGNED_BYTE;
    } else if (format == GL_RGBA8) {
        channel_format = GL_RGBA;
        channel_type = GL_UNSIGNED_BYTE;
    } else if (format == GL_SRGB8_ALPHA8) {
        channel_format = GL_SRGB_ALPHA;
        channel_type = GL_UNSIGNED_BYTE;
    }
}

}

GlBuffer::GlBuffer(uint64_t size, uint32_t usage, const void *data) : size_(size) {
    if ((usage & GL_MAP_WRITE_BIT) != 0) {
        usage |= GL_MAP_READ_BIT;
    }
    glCreateBuffers(1, &gl_buffer_);
    glNamedBufferStorage(gl_buffer_, size, data, usage);
}

GlBuffer::~GlBuffer() {
    unmap();
    glDeleteBuffers(1, &gl_buffer_);
}

void *GlBuffer::map(bool write) {
    if (!mapped_ptr_) {
        mapped_ptr_ = glMapNamedBuffer(gl_buffer_, write ? GL_READ_WRITE : GL_READ_ONLY);
    }
    return mapped_ptr_;
}

void GlBuffer::unmap() {
    if (mapped_ptr_) {
        glUnmapNamedBuffer(gl_buffer_);
        glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
        mapped_ptr_ = nullptr;
    }
}

GlTexture2D::GlTexture2D(uint32_t format, uint32_t width, uint32_t height, uint32_t levels)
    : width_(width), height_(height), levels_(levels), format_(format) {
    if (levels == 0) {
        uint32_t temp_width = width;
        uint32_t temp_height = height;
        levels_ = 1;
        while (temp_width > 1 || temp_height > 1) {
            ++levels_;
            temp_width = temp_width == 1 ? 1 : temp_width / 2;
            temp_height = temp_height == 1 ? 1 : temp_height / 2;
        }
    }
    get_channel_format_type(format, channel_format_, channel_type_);
    glCreateTextures(GL_TEXTURE_2D, 1, &gl_texture_);
    glTextureStorage2D(gl_texture_, levels_, format, width, height);
    glTextureParameteri(gl_texture_, GL_TEXTURE_MIN_FILTER, levels_ == 1 ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(gl_texture_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(gl_texture_, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(gl_texture_, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

GlTexture2D::~GlTexture2D() {
    glDeleteTextures(1, &gl_texture_);
}

void GlTexture2D::set_data(const void *data, uint32_t level) {
    auto level_width = std::max(1u, width_ >> level);
    auto level_height = std::max(1u, height_ >> level);
    glTextureSubImage2D(gl_texture_, level, 0, 0, level_width, level_height, channel_format_, channel_type_, data);
}

void GlTexture2D::generate_mipmap() {
    glGenerateTextureMipmap(gl_texture_);
}
