#include "program.hpp"

#include <string>
#include <iostream>

#include <glad/glad.h>

GlShader::GlShader(const char *source, uint32_t type) {
    shader_ = glCreateShader(type);
    const char *p_code = source;
    glShaderSource(shader_, 1, &p_code, nullptr);
    glCompileShader(shader_);

    int ret;
    glGetShaderiv(shader_, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        char log_info[512];
        glGetShaderInfoLog(shader_, 512, nullptr, log_info);
        std::string sh_str;
        if (type == GL_VERTEX_SHADER) {
            sh_str = "vertex";
        } else if (type == GL_FRAGMENT_SHADER) {
            sh_str = "fragment";
        } else if (type == GL_COMPUTE_SHADER) {
            sh_str = "compute";
        }
        std::cerr << "CE on " << sh_str << " shader: " << log_info << std::endl;
        glDeleteShader(shader_);
        shader_ = 0;
    }
}

GlShader::GlShader(const uint8_t *binary, uint32_t length, uint32_t type) {
    shader_ = glCreateShader(type);
    glShaderBinary(1, &shader_, GL_SHADER_BINARY_FORMAT_SPIR_V, binary, length);
    glSpecializeShader(shader_, "main", 0, nullptr, nullptr);

    int ret;
    glGetShaderiv(shader_, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        char log_info[512];
        glGetShaderInfoLog(shader_, 512, nullptr, log_info);
        std::string sh_str;
        if (type == GL_VERTEX_SHADER) {
            sh_str = "vertex";
        } else if (type == GL_FRAGMENT_SHADER) {
            sh_str = "fragment";
        } else if (type == GL_COMPUTE_SHADER) {
            sh_str = "compute";
        }
        std::cerr << "CE on " << sh_str << " shader: " << log_info << std::endl;
        glDeleteShader(shader_);
        shader_ = 0;
    }
}

GlShader::~GlShader() {
    if (shader_) {
        glDeleteShader(shader_);
        shader_ = 0;
    }
}


GlGraphicsProgram::GlGraphicsProgram(const GlShader &vs, const GlShader &fs) {
    program_ = glCreateProgram();
    glAttachShader(program_, vs.id());
    glAttachShader(program_, fs.id());

    glLinkProgram(program_);
    int ret = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ret);
    if (!ret) {
        char log_info[512];
        glGetProgramInfoLog(program_, 512, nullptr, log_info);
        std::cerr << "link graphics program error: " << log_info << std::endl;

        glDeleteProgram(program_);
        program_ = 0;
    }

    glDetachShader(program_, vs.id());
    glDetachShader(program_, fs.id());
}

GlGraphicsProgram::~GlGraphicsProgram() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}


GlComputeProgram::GlComputeProgram(const GlShader &cs) {
    program_ = glCreateProgram();
    glAttachShader(program_, cs.id());

    glLinkProgram(program_);
    int ret = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ret);
    if (!ret) {
        char log_info[512];
        glGetProgramInfoLog(program_, 512, nullptr, log_info);
        std::cerr << "link compute program error: " << log_info << std::endl;

        glDeleteProgram(program_);
        program_ = 0;
    }

    glDetachShader(program_, cs.id());
}

GlComputeProgram::~GlComputeProgram() {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}
