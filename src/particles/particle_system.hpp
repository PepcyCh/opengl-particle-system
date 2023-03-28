#pragma once

#include <memory>
#include <random>

#include <glm/glm.hpp>

#include "../glh/resource.hpp"
#include "../glh/program.hpp"

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    void set_camera_buffer(const GlBuffer *camera_buffer) { camera_buffer_ = camera_buffer; }

    void update(float delta_time);

private:
    void init_pipeline_emit();
    void init_pipeline_update();
    void init_pipeline_compact();
    void init_pipeline_draw();

    void draw_ui();
    void do_emit();
    void do_update(float delta_time);
    void do_compact();
    void do_draw();

    std::mt19937 rng_;

    struct {
        uint32_t emit_interval = 1;
        uint32_t compact_interval = 1;
        uint32_t count_min = 1;
        uint32_t count_max = 1;
        glm::vec3 position = glm::vec3(0.0f);
        float position_radius = 0.0f;
        glm::vec3 velocity = glm::vec3(0.0f, 1.0f, 0.0f);
        float velocity_angle = 180.0f;
        float life_min = 1.0f;
        float life_max = 1.0f;
        float mass_min = 1.0f;
        float mass_max = 1.0f;
        float size_min = 0.05f;
        float size_max = 0.05f;
    } emit_settings_;
    struct {
        glm::vec3 force = glm::vec3(0.0f);
        float gravity = 9.8f;
        float drag = 0.0f;
    } update_settings_;
    struct {
        glm::vec4 color = glm::vec4(1.0f);
    } render_settings_;

    bool executing_ = true;
    uint32_t emit_counter_ = 0;
    uint32_t compact_counter_ = 0;

    uint32_t num_particles_ = 0;
    uint32_t curr_particles_index_ = 0;
    std::unique_ptr<GlBuffer> particles_buffer_[2];

    std::unique_ptr<GlComputeProgram> emit_program_;
    std::unique_ptr<GlBuffer> emit_settings_buffer_;
    std::unique_ptr<GlBuffer> emit_params_buffer_;
    bool emit_settings_dirty_ = true;
    uint32_t emit_seed_ = 0;

    std::unique_ptr<GlComputeProgram> update_program_;
    std::unique_ptr<GlBuffer> update_params_buffer_;

    std::unique_ptr<GlComputeProgram> scan1_program_;
    std::unique_ptr<GlComputeProgram> scan2_program_;
    std::unique_ptr<GlComputeProgram> scan3_program_;
    std::unique_ptr<GlComputeProgram> compact_program_;
    std::unique_ptr<GlBuffer> compact_indices_buffer_;
    std::unique_ptr<GlBuffer> scan_buffer_[2];
    std::unique_ptr<GlBuffer> scan_params_buffer_[2];

    std::unique_ptr<GlGraphicsProgram> draw_program_;
    std::unique_ptr<GlBuffer> draw_params_buffer_;
    bool render_settings_dirty_ = true;
    uint32_t draw_vao_ = 0;
    std::unique_ptr<GlBuffer> billboard_index_buffer_;
    std::unique_ptr<GlTexture2D> billboard_tex_;

    const GlBuffer *camera_buffer_ = nullptr;
};
