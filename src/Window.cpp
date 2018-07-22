#include <iostream>
using std::cout;
using std::endl;
#include <stdexcept>
using std::runtime_error;

#include "Window.h"
#include "AudioProcess.h" // VISUALIZER_BUFSIZE

Window::Window(int _width, int _height) : width(_width), height(_height), size_changed(true), mouse() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_DECORATED, false);

	window = glfwCreateWindow(width, height, "Music Visualizer", NULL, NULL);
	if (window == NULL) throw runtime_error("GLFW window creation failed.");

	glfwMakeContextCurrent(window);
	glfwSetWindowUserPointer(window, this);
	auto mouse_button_func = [](GLFWwindow * window, int button, int action, int mods) {
		static_cast<Window*>(glfwGetWindowUserPointer(window))->mouse_button_callback(button, action, mods);
	};
	auto cursor_pos_func = [](GLFWwindow * window, double xpos, double ypos) {
		static_cast<Window*>(glfwGetWindowUserPointer(window))->cursor_position_callback(xpos, ypos);
	};
	auto window_size_func = [](GLFWwindow * window, int width, int height) {
		static_cast<Window*>(glfwGetWindowUserPointer(window))->window_size_callback(width, height);
	};
	auto keyboard_func = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		static_cast<Window*>(glfwGetWindowUserPointer(window))->keyboard_callback(key, scancode, action, mods);
	};
	glfwSetKeyCallback(window, keyboard_func);
	glfwSetCursorPosCallback(window, cursor_pos_func);
	glfwSetMouseButtonCallback(window, mouse_button_func);
	glfwSetWindowSizeCallback(window, window_size_func);

	glewExperimental = GL_TRUE;
	glewInit();
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported "<< version << endl;

	glfwSwapInterval(0);
}

Window::~Window() {
	// If we're being destroyed, then the app is shutting down.
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::window_size_callback(int _width, int _height) {
	width = _width;
	height = _height;
	size_changed = true;
}

void Window::cursor_position_callback(double xpos, double ypos) {
    mouse.x = float(xpos);
    mouse.y = height - float(ypos);
    if (mouse.down) {
        mouse.last_down_x = mouse.x;
        mouse.last_down_y = mouse.y;
    }
}

void Window::mouse_button_callback(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        mouse.down = true;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		mouse.down = false;
    if (mouse.down) {
        mouse.last_down_x = mouse.x;
        mouse.last_down_y = mouse.y;
    }
}

void Window::keyboard_callback(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

bool Window::is_alive() {
	return !glfwWindowShouldClose(window); 
}

void Window::poll_events() {
	// size_changed should've been noticed by renderer this frame, so reset
	size_changed = false;
	glfwPollEvents();
}

void Window::swap_buffers() {
	glfwSwapBuffers(window);
}