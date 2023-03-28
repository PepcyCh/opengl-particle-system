#pragma once

#include <filesystem>

class GlShader {
public:
    GlShader(const char *source, uint32_t type);
    GlShader(const uint8_t *binary, uint32_t length, uint32_t type);
    ~GlShader();

    uint32_t id() const { return shader_; }

private:
    uint32_t shader_ = 0;
};

class GlGraphicsProgram {
public:
    GlGraphicsProgram(const GlShader &vs, const GlShader &fs);
    ~GlGraphicsProgram();

    uint32_t id() const { return program_; }

private:
    uint32_t program_ = 0;
};

class GlComputeProgram {
public:
    GlComputeProgram(const GlShader &cs);
    ~GlComputeProgram();

    uint32_t id() const { return program_; }

private:
    uint32_t program_ = 0;
};
