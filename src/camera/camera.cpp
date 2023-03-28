#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "camera.hpp"

#include <algorithm>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct ShaderCamera {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_inv;
};

constexpr float kPi = 3.141592653589793238463f;

constexpr float kZNear = 0.001f;
constexpr float kZFar = 100000.0f;

}

OrbitCamera::OrbitCamera(const glm::vec3 &look_at, float radius, float aspect) : look_at_(look_at), radius_(radius) {
    theta_ = 0.0f;
    phi_ = kPi * 0.5f;
    proj_ = glm::perspective(glm::radians(45.0f), aspect, kZNear, kZFar);
    proj_inv_ = glm::inverse(proj_);
    update();

    camera_buffer_ = std::make_unique<GlBuffer>(sizeof(ShaderCamera), GL_MAP_WRITE_BIT);
}

void OrbitCamera::rotate(float delta_x, float delta_y) {
    theta_ -= delta_x;
    if (theta_ < 0.0) {
        theta_ += 2.0f * kPi;
    } else if (theta_ >= 2.0f * kPi) {
        theta_ -= 2.0f * kPi;
    }
    phi_ = std::clamp(phi_ + delta_y, 0.1f, kPi - 0.1f);
    update();
}

void OrbitCamera::forward(float delta) {
    radius_ = std::max(radius_ + delta, 0.1f);
    update();
}

void OrbitCamera::set_aspect(float aspect) {
    proj_ = glm::perspective(glm::radians(45.0f), aspect, kZNear, kZFar);
    proj_inv_ = glm::inverse(proj_);
    update();
}

void OrbitCamera::update() {
    pos_.x = radius_ * std::sin(phi_) * std::cos(theta_);
    pos_.y = radius_ * std::cos(phi_);
    pos_.z = radius_ * std::sin(phi_) * std::sin(theta_);
    view_ = glm::lookAt(pos_, look_at_, glm::vec3(0.0f, 1.0f, 0.0f));
    view_inv_ = glm::inverse(view_);
    camera_buffer_dirty_ = true;
}

const GlBuffer *OrbitCamera::get_buffer() {
    if (camera_buffer_dirty_) {
        auto data = camera_buffer_->typed_map<ShaderCamera>(true);
        data->view = view_;
        data->view_inv = view_inv_;
        data->proj = proj_;
        camera_buffer_->unmap();
        camera_buffer_dirty_ = false;
    }
    return camera_buffer_.get();
}
