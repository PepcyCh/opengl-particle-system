#pragma once

#include <functional>

#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <imgui.h>

class Window {
public:
    Window(uint32_t width, uint32_t height, const char *title);
    ~Window();

    float get_aspect() const { return static_cast<float>(width_) / height_; }

    void main_loop(const std::function<void()> &func);

    using MouseCallback = std::function<void(uint32_t, float, float, float, float)>;
    enum Mouse {
        eMouseLeft = 1,
        eMouseRight = 2,
        eMouseMiddle = 4,
    };
    void set_mouse_callback(const MouseCallback &callback);

    using KeyCallback = std::function<void(int, int)>;
    void set_key_callback(const KeyCallback &callback);

    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
    void set_resize_callback(const ResizeCallback &callback);

    void set_title(const char *title);

private:
    friend void glfw_cursor_pos_callback(GLFWwindow *window, double x, double y);
    friend void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
    friend void glfw_resize_callback(GLFWwindow *window, int width, int height);

    uint32_t width_;
    uint32_t height_;

    GLFWwindow *window_ = nullptr;

    MouseCallback mouse_callback_ = [](uint32_t, float, float, float, float) {};
    float last_mouse_x_ = 0.0f;
    float last_mouse_y_ = 0.0f;

    KeyCallback key_callback_ = [](int, int) {};

    ResizeCallback resize_callback_ = [](uint32_t, uint32_t) {};

    std::unordered_map<float, ImFont *> imgui_fonts_;
    ImFont *imgui_curr_font_ = nullptr;
    float imgui_last_scale_ = 1.0f;
};
