#pragma once

#include "gl.h"
#include <GLFW/glfw3.h>

class Window {
public:
	Window(int width, int height);
	~Window();
	void window_size_callback(GLFWwindow* window, int width, int height);
	void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
	void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

private:
	GLFWwindow* window;
	int width;
	int height;
};
