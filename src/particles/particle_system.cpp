#include "particle_system.hpp"

#include <numbers>

#include <cmrc/cmrc.hpp>
#include <glad/glad.h>
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

CMRC_DECLARE(shaders_spv);
CMRC_DECLARE(assets);

namespace {

constexpr uint32_t kScanWidth = 512;
constexpr uint32_t kMaxNumParticles = kScanWidth * kScanWidth;

struct alignas(16) Particle {
    glm::vec3 position;
    float mass;
    glm::vec3 velocity;
    float life;
    glm::vec3 acceleration;
    float size;
};

struct alignas(16) ParticleEmissionSettings {
    glm::vec3 position;
    float position_radius;
    glm::vec3 velocity;
    float velocity_angle_cos;
    float mass_min;
    float mass_max;
    float life_min;
    float life_max;
    float size_min;
    float size_max;
};
struct EmitParams {
    uint32_t offset;
    uint32_t count;
    uint32_t seed;
};

struct alignas(16) UpdateParams {
    glm::vec3 force;
    float delta_time;
    uint32_t num_particles;
    float gravity;
    float drag;
};

struct alignas(16) RenderParams {
    glm::vec4 color;
};

void build_compute_program(std::unique_ptr<GlComputeProgram> &program, const char *spv_path) {
    auto spv_file = cmrc::shaders_spv::get_filesystem().open(spv_path);
    assert(spv_file.size() > 0 && spv_file.size() % 4 == 0);
    std::vector<uint8_t> spv_data(spv_file.size());
    std::copy(spv_file.begin(), spv_file.end(), spv_data.data());
    GlShader shader(spv_data.data(), static_cast<uint32_t>(spv_file.size()), GL_COMPUTE_SHADER);
    program = std::make_unique<GlComputeProgram>(shader);
}

void build_graphics_program(
    std::unique_ptr<GlGraphicsProgram> &program, const char *vs_spv_path, const char *fs_spv_path
) {
    auto vs_spv_file = cmrc::shaders_spv::get_filesystem().open(vs_spv_path);
    assert(vs_spv_file.size() > 0 && vs_spv_file.size() % 4 == 0);
    std::vector<uint8_t> vs_spv_data(vs_spv_file.size());
    std::copy(vs_spv_file.begin(), vs_spv_file.end(), vs_spv_data.data());
    GlShader vs_shader(vs_spv_data.data(), static_cast<uint32_t>(vs_spv_file.size()), GL_VERTEX_SHADER);
    
    auto fs_spv_file = cmrc::shaders_spv::get_filesystem().open(fs_spv_path);
    assert(fs_spv_file.size() > 0 && fs_spv_file.size() % 4 == 0);
    std::vector<uint8_t> fs_spv_data(fs_spv_file.size());
    std::copy(fs_spv_file.begin(), fs_spv_file.end(), fs_spv_data.data());
    GlShader fs_shader(fs_spv_data.data(), static_cast<uint32_t>(fs_spv_file.size()), GL_FRAGMENT_SHADER);

    program = std::make_unique<GlGraphicsProgram>(vs_shader, fs_shader);
}

void read_texture(std::unique_ptr<GlTexture2D> &texture, const char *path) {
    auto file = cmrc::assets::get_filesystem().open(path);
    std::vector<uint8_t> file_data(file.size());
    std::copy(file.begin(), file.end(), file_data.data());

    int num_channels;
    int width;
    int height;
    auto img_data = stbi_load_from_memory(file_data.data(), file.size(), &width, &height, &num_channels, 0);

    if (num_channels == 4) {
        texture = std::make_unique<GlTexture2D>(GL_RGBA8, width, height);
        texture->set_data(img_data);
        texture->generate_mipmap();
    } else if (num_channels == 3) {
        texture = std::make_unique<GlTexture2D>(GL_RGB8, width, height);
        texture->set_data(img_data);
        texture->generate_mipmap();
    } else {
        static const uint8_t default_tex_data[] = { 255, 255, 255, 255 };
        texture = std::make_unique<GlTexture2D>(GL_RGBA8, 1, 1, 1);
        texture->set_data(default_tex_data);
    }
    stbi_image_free(img_data);
}

}

ParticleSystem::ParticleSystem() : rng_(std::random_device{}()) {
    particles_buffer_[0] = std::make_unique<GlBuffer>(kMaxNumParticles * sizeof(Particle));
    particles_buffer_[1] = std::make_unique<GlBuffer>(kMaxNumParticles * sizeof(Particle));

    init_pipeline_emit();
    init_pipeline_update();
    init_pipeline_compact();
    init_pipeline_draw();
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(1, &draw_vao_);
}

void ParticleSystem::update(float delta_time) {
    draw_ui();

    if (executing_) {
        if (emit_settings_.emit_interval > 0 && ++emit_counter_ == emit_settings_.emit_interval) {
            do_emit();
            emit_counter_ = 0;
        }
        if (num_particles_ > 0) {
            do_update(delta_time);
            if (emit_settings_.compact_interval > 0 && ++compact_counter_ == emit_settings_.compact_interval) {
                do_compact();
                compact_counter_ = 0;
            }
        }
    }
    if (num_particles_ > 0) {
        do_draw();
    }

    glUseProgram(0);
}

void ParticleSystem::init_pipeline_emit() {
    build_compute_program(emit_program_, "particle/emit.comp.spv");

    emit_settings_buffer_ = std::make_unique<GlBuffer>(sizeof(ParticleEmissionSettings), GL_MAP_WRITE_BIT);
    emit_params_buffer_ = std::make_unique<GlBuffer>(sizeof(EmitParams), GL_MAP_WRITE_BIT);
}

void ParticleSystem::init_pipeline_update() {
    build_compute_program(update_program_, "particle/update.comp.spv");

    update_params_buffer_ = std::make_unique<GlBuffer>(sizeof(UpdateParams), GL_MAP_WRITE_BIT);
}

void ParticleSystem::init_pipeline_compact() {
    build_compute_program(compact_program_, "particle/compact.comp.spv");
    build_compute_program(scan1_program_, "particle/scan1.comp.spv");
    build_compute_program(scan2_program_, "particle/scan2.comp.spv");
    build_compute_program(scan3_program_, "particle/scan3.comp.spv");

    compact_indices_buffer_ = std::make_unique<GlBuffer>(kMaxNumParticles * sizeof(uint32_t));
    scan_buffer_[0] = std::make_unique<GlBuffer>(kScanWidth * sizeof(uint32_t));
    scan_buffer_[1] = std::make_unique<GlBuffer>(sizeof(uint32_t), GL_MAP_READ_BIT);
    scan_params_buffer_[0] = std::make_unique<GlBuffer>(sizeof(uint32_t), GL_MAP_WRITE_BIT);
    scan_params_buffer_[1] = std::make_unique<GlBuffer>(sizeof(uint32_t), GL_MAP_WRITE_BIT);
}

void ParticleSystem::init_pipeline_draw() {
    build_graphics_program(draw_program_, "particle/draw.vert.spv", "particle/draw.frag.spv");

    draw_params_buffer_ = std::make_unique<GlBuffer>(sizeof(RenderParams), GL_MAP_WRITE_BIT);

    glCreateVertexArrays(1, &draw_vao_);

    const uint32_t billboard_index[] = { 0, 1, 2, 0, 2, 3 };
    billboard_index_buffer_ = std::make_unique<GlBuffer>(sizeof(billboard_index), 0, billboard_index);
    glVertexArrayElementBuffer(draw_vao_, billboard_index_buffer_->id());

    read_texture(billboard_tex_, "assets/circle.png");
}

void ParticleSystem::draw_ui() {
    if (ImGui::Begin("Particle System")) {
        ImGui::Text("num particles: %u", num_particles_);

        if (executing_) {
            executing_ = !ImGui::Button("pause");
        } else {
            executing_ = ImGui::Button("resume");
        }

        ImGui::Separator();
        ImGui::Text("emit");

        emit_settings_dirty_ |= ImGui::DragInt(
            "emit interval", reinterpret_cast<int *>(&emit_settings_.emit_interval),
            1.0f, 0, 10
        );
        emit_settings_dirty_ |= ImGui::DragInt(
            "compact interval", reinterpret_cast<int *>(&emit_settings_.compact_interval),
            1.0f, 0, 10
        );

        emit_settings_dirty_ |= ImGui::DragIntRange2(
            "num emitted",
            reinterpret_cast<int *>(&emit_settings_.count_min),
            reinterpret_cast<int *>(&emit_settings_.count_max),
            1.0f, 0, kMaxNumParticles
        );

        emit_settings_dirty_ |= ImGui::DragFloat3(
            "position", &emit_settings_.position.x, 0.05f, -100.0f, 100.0f
        );
        emit_settings_dirty_ |= ImGui::DragFloat(
            "radius", &emit_settings_.position_radius, 0.05f, 0.0f, 100.0f
        );
        emit_settings_dirty_ |= ImGui::DragFloat3(
            "velocity", &emit_settings_.velocity.x, 0.05f, -100.0f, 100.0f
        );
        emit_settings_dirty_ |= ImGui::DragFloat(
            "angle", &emit_settings_.velocity_angle, 1.0f, 0.0f, 180.0f
        );

        emit_settings_dirty_ |= ImGui::DragFloatRange2(
            "life", &emit_settings_.life_min, &emit_settings_.life_max,
            0.1f, 0.01f, 1000.0f
        );
        emit_settings_dirty_ |= ImGui::DragFloatRange2(
            "mass", &emit_settings_.mass_min, &emit_settings_.mass_max,
            0.01f, 0.01f, 100.0f
        );
        emit_settings_dirty_ |= ImGui::DragFloatRange2(
            "size", &emit_settings_.size_min, &emit_settings_.size_max,
            0.01f, 0.01f, 100.0f
        );

        ImGui::Separator();
        ImGui::Text("update");

        ImGui::DragFloat3("force", &update_settings_.force.x, 0.01f, -100.0f, 100.0f);
        ImGui::DragFloat("gravity", &update_settings_.gravity, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("drag", &update_settings_.drag, 0.01f, 0.0f, 100.0f);

        ImGui::Separator();
        ImGui::Text("render");

        render_settings_dirty_ |= ImGui::ColorEdit4("color", &render_settings_.color.x);
    }
    ImGui::End();
}

void ParticleSystem::do_emit() {
    if (emit_settings_dirty_) {
        auto data = emit_settings_buffer_->typed_map<ParticleEmissionSettings>(true);
        data->position = emit_settings_.position;
        data->position_radius = emit_settings_.position_radius;
        data->velocity = emit_settings_.velocity;
        data->velocity_angle_cos = std::cos(emit_settings_.velocity_angle / 180.0f * std::numbers::pi);
        data->life_min = emit_settings_.life_min;
        data->life_max = emit_settings_.life_max;
        data->mass_min = emit_settings_.mass_min;
        data->mass_max = emit_settings_.mass_max;
        data->size_min = emit_settings_.size_min;
        data->size_max = emit_settings_.size_max;
        emit_settings_buffer_->unmap();
        emit_settings_dirty_ = false;
    }

    std::uniform_real_distribution<> rng01(0.0f, 1.0f);
    uint32_t num_emitted =
        emit_settings_.count_min + rng01(rng_) * (emit_settings_.count_max - emit_settings_.count_min);
    num_emitted = std::min(num_emitted, kMaxNumParticles - num_particles_);
    if (num_emitted == 0) {
        return;
    }

    {
        auto data = emit_params_buffer_->typed_map<EmitParams>(true);
        data->offset = num_particles_;
        data->count = num_emitted;
        data->seed = emit_seed_++;
        emit_params_buffer_->unmap();
    }
    num_particles_ += num_emitted;

    glUseProgram(emit_program_->id());
    uint32_t buffers[] = {
        particles_buffer_[curr_particles_index_]->id(),
        emit_settings_buffer_->id(),
        emit_params_buffer_->id(),
    };
    glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 1, buffers);
    glBindBuffersBase(GL_UNIFORM_BUFFER, 1, 2, buffers + 1);

    glDispatchCompute((num_emitted + 255) / 256, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ParticleSystem::do_update(float delta_time) {
    {
        auto data = update_params_buffer_->typed_map<UpdateParams>(true);
        data->delta_time = delta_time;
        data->num_particles = num_particles_;
        data->force = update_settings_.force;
        data->gravity = update_settings_.gravity;
        data->drag = update_settings_.drag;
        update_params_buffer_->unmap();
    }

    glUseProgram(update_program_->id());
    uint32_t buffers[] = {
        particles_buffer_[curr_particles_index_]->id(),
        update_params_buffer_->id(),
    };
    glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 1, buffers);
    glBindBuffersBase(GL_UNIFORM_BUFFER, 1, 1, buffers + 1);

    glDispatchCompute((num_particles_ + 255) / 256, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ParticleSystem::do_compact() {
    auto num_blocks = (num_particles_ + 511) / 512;
    {
        auto data0 = scan_params_buffer_[0]->typed_map<uint32_t>(true);
        *data0 = num_particles_;
        scan_params_buffer_[0]->unmap();

        auto data1 = scan_params_buffer_[1]->typed_map<uint32_t>(true);
        *data1 = num_blocks;
        scan_params_buffer_[1]->unmap();
    }

    // scan 1
    {
        glUseProgram(scan1_program_->id());
        uint32_t buffers[] = {
            particles_buffer_[curr_particles_index_]->id(),
            compact_indices_buffer_->id(),
            scan_buffer_[0]->id(),
            scan_params_buffer_[0]->id(),
        };
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 3, buffers);
        glBindBuffersBase(GL_UNIFORM_BUFFER, 3, 1, buffers + 3);

        glDispatchCompute(num_blocks, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // scan 2
    {
        glUseProgram(scan2_program_->id());
        uint32_t buffers[] = {
            scan_buffer_[0]->id(),
            scan_buffer_[1]->id(),
            scan_params_buffer_[1]->id(),
        };
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 2, buffers);
        glBindBuffersBase(GL_UNIFORM_BUFFER, 2, 1, buffers + 2);

        glDispatchCompute(1, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // scan 3
    if (num_blocks > 1) {
        glUseProgram(scan3_program_->id());
        uint32_t buffers[] = {
            compact_indices_buffer_->id(),
            scan_buffer_[0]->id(),
            scan_params_buffer_[0]->id(),
        };
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 2, buffers);
        glBindBuffersBase(GL_UNIFORM_BUFFER, 2, 1, buffers + 2);

        glDispatchCompute(num_blocks - 1, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // compact
    {
        glUseProgram(compact_program_->id());
        uint32_t buffers[] = {
            particles_buffer_[curr_particles_index_]->id(),
            compact_indices_buffer_->id(),
            particles_buffer_[curr_particles_index_ ^ 1]->id(),
            scan_params_buffer_[0]->id(),
        };
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 3, buffers);
        glBindBuffersBase(GL_UNIFORM_BUFFER, 3, 1, buffers + 3);

        glDispatchCompute(num_blocks, 1, 1);
    }

    curr_particles_index_ ^= 1;

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    auto fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glDeleteSync(fence);

    auto data = scan_buffer_[1]->typed_map<uint32_t>();
    num_particles_ = *data;
    scan_buffer_[1]->unmap();
}

void ParticleSystem::do_draw() {
    if (render_settings_dirty_) {
        auto data = draw_params_buffer_->typed_map<RenderParams>(true);
        data->color = render_settings_.color;
        draw_params_buffer_->unmap();
        render_settings_dirty_ = false;
    }

    // glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(draw_program_->id());
    uint32_t buffers[] = {
        particles_buffer_[curr_particles_index_]->id(),
        camera_buffer_->id(),
        draw_params_buffer_->id(),
    };
    glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 1, buffers);
    glBindBuffersBase(GL_UNIFORM_BUFFER, 1, 2, buffers + 1);
    glBindTextureUnit(3, billboard_tex_->id());
    glBindVertexArray(draw_vao_);

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, num_particles_);

    glBindVertexArray(0);
}
