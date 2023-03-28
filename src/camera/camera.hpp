#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "../glh/resource.hpp"

class OrbitCamera {
public:
    OrbitCamera(const glm::vec3 &look_at, float radius, float aspect);

    glm::vec3 position() const { return pos_; }
    glm::mat4 proj() const { return proj_; }
    glm::mat4 proj_inv() const { return proj_inv_; }
    glm::mat4 view() const { return view_; }
    glm::mat4 view_inv() const { return view_inv_; }

    void rotate(float delta_x, float delta_y);
    void forward(float delta);

    void set_aspect(float aspect);

    const GlBuffer *get_buffer();

private:
    void update();

    glm::vec3 pos_;
    glm::vec3 look_at_;
    float theta_;
    float phi_;
    float radius_;
    glm::mat4 proj_;
    glm::mat4 proj_inv_;
    glm::mat4 view_;
    glm::mat4 view_inv_;

    std::unique_ptr<GlBuffer> camera_buffer_;
    bool camera_buffer_dirty_ = true;
};
