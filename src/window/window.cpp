#include "window.hpp"

#include <iostream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace {

void glfw_error_callback(int error, const char *desc) {
    std::cerr << "glfw error: " << desc << std::endl;
}

void GLAPIENTRY gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar *message, const void *user_param) {
    if (severity != GL_DEBUG_SEVERITY_HIGH) {
        return;
    }
    auto type_str = type == GL_DEBUG_TYPE_ERROR ? "[ERROR]"
        : type == GL_DEBUG_TYPE_PERFORMANCE ? "[PERFORMANCE]"
        : type == GL_DEBUG_TYPE_MARKER ? "[MARKER]"
        : type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ? "[DEPRECATED]"
        : type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ? "[UNDEFINED BEHAVIOR]" : "[OTHER]";
    auto severity_str = severity == GL_DEBUG_SEVERITY_HIGH ? "[HIGH]"
        : severity == GL_DEBUG_SEVERITY_MEDIUM ? "[MEDIUM]"
        : severity == GL_DEBUG_SEVERITY_LOW ? "[LOW]" : "[NOTIFICATION]";
    std::cerr << "OpenGL callback " << type_str << " " << severity_str << " " << message << std::endl;
    if (type == GL_DEBUG_TYPE_ERROR && severity == GL_DEBUG_SEVERITY_HIGH) {
        // exit(-1);
    }
}

constexpr float kImguiFontSize = 14.0f;

}

void glfw_cursor_pos_callback(GLFWwindow *glfw_window, double x, double y) {
    auto window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
    uint32_t state = 0;
    if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        state |= Window::eMouseLeft;
    }
    if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        state |= Window::eMouseRight;
    }
    if (glfwGetMouseButton(glfw_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        state |= Window::eMouseMiddle;
    }
    window->mouse_callback_(state, x, y, window->last_mouse_x_, window->last_mouse_y_);
    window->last_mouse_x_ = x;
    window->last_mouse_y_ = y;
}

void glfw_key_callback(GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    auto window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
    window->key_callback_(key, action);
}

void glfw_resize_callback(GLFWwindow *glfw_window, int width, int height) {
    auto window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
    glViewport(0, 0, width, height);

    float width_scale, height_scale;
    glfwGetWindowContentScale(glfw_window, &width_scale, &height_scale);

    auto dpi_scale = std::max(width_scale, height_scale);
    auto rel_scale = dpi_scale / window->imgui_last_scale_;
    ImGui::GetStyle().ScaleAllSizes(rel_scale);
    window->imgui_last_scale_ = dpi_scale;
    if (auto it = window->imgui_fonts_.find(dpi_scale); it != window->imgui_fonts_.end()) {
        window->imgui_curr_font_ = it->second;
    } else {
        ImGuiIO &io = ImGui::GetIO();
        ImFontConfig font_cfg {};
        font_cfg.SizePixels = kImguiFontSize * dpi_scale;
        auto font = io.Fonts->AddFontDefault(&font_cfg);
        window->imgui_fonts_[dpi_scale] = font;
        window->imgui_curr_font_ = font;
        io.Fonts->Build();
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
    }

    width /= width_scale;
    height /= height_scale;
    if (window->width_ != width || window->height_ != height) {
        window->width_ = width;
        window->height_ = height;
        window->resize_callback_(width, height);
    }
}

Window::Window(uint32_t width, uint32_t height, const char *title) : width_(width), height_(height) {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) {
        std::cerr << "Failed to create glfw window\n";
        return;
    }
    glfwMakeContextCurrent(window_);

    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, glfw_cursor_pos_callback);
    glfwSetKeyCallback(window_, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window_, glfw_resize_callback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to load OpenGL functions\n";
    }

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(gl_message_callback, nullptr);

    int frame_width, frame_height;
    glfwGetFramebufferSize(window_, &frame_width, &frame_height);
    glViewport(0, 0, frame_width, frame_height);

    float width_scale, height_scale;
    glfwGetWindowContentScale(window_, &width_scale, &height_scale);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    auto dpi_scale = std::max(width_scale, height_scale);
    ImFontConfig font_cfg {};
    font_cfg.SizePixels = kImguiFontSize * dpi_scale;
    imgui_curr_font_ = io.Fonts->AddFontDefault(&font_cfg);
    imgui_fonts_[dpi_scale] = imgui_curr_font_;

    ImGui::StyleColorsClassic();
    imgui_last_scale_ = dpi_scale;
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::cout << "GL_VERSION: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GL_RENDERER: " << glGetString(GL_RENDERER) << std::endl;
}

Window::~Window() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Window::main_loop(const std::function<void()> &func) {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::PushFont(imgui_curr_font_);
        func();
        ImGui::PopFont();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void Window::set_mouse_callback(const MouseCallback &callback) {
    mouse_callback_ = callback;
}

void Window::set_key_callback(const KeyCallback &callback) {
    key_callback_ = callback;
}

void Window::set_resize_callback(const ResizeCallback &callback) {
    resize_callback_ = callback;
}

void Window::set_title(const char *title) {
    glfwSetWindowTitle(window_, title);
}
