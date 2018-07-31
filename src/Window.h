#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Window {
public:
    Window(int width, int height);
    ~Window();

    void poll_events();
    void swap_buffers();
    bool is_alive();

    int width;
    int height;
    bool size_changed;
    struct {
        float x;
        float y;
        bool down;
        float last_down_x;
        float last_down_y;
    } mouse;

private:
    GLFWwindow* window;

    void window_size_callback(int width, int height);
    void cursor_position_callback(double xpos, double ypos);
    void mouse_button_callback(int button, int action, int mods);
    void keyboard_callback(int key, int scancode, int action, int mods);
};
