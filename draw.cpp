/* Useful links about geometry shaders
 * https://open.gl/geometry
 * https://www.khronos.org/opengl/wiki/Geometry_Shader
 * http://www.informit.com/articles/article.aspx?p=2120983&seqNum=2
 */

// TODO Test the output of the shaders. Use dummy data in audio_data. Compute similarity between expected images
// and produced images.

#include <iostream>
#include <vector>
using std::vector;
using std::cout;
using std::endl;
#include <chrono>

#include <fstream>
#include <sstream>
#include <filesystem>
namespace filesys = std::experimental::filesystem;

#include <string.h>

#include "draw.h"
#include "audio_data.h"

#ifdef WINDOWS
#include "glew.h"
#endif
#ifdef LINUX
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>
static GLFWwindow* window;

// TODO Currently I have to toggle the following two lines in order to use different shaders. Yeah, this whole file is really just a scratch pad
// Use the greater number of points to render the waveform and lissajous using the lines shader, use the smaller number of points to render the fullscreen quads
static const int POINTS = VISUALIZER_BUFSIZE - 1;
// static const int POINTS = 1;
static int max_output_vertices;

// window dimensions
static int window_width = 1000;
static int window_height = 400;

static GLuint tex[4];    // textures
static GLint tex_loc[4]; // texture uniform locations
static GLuint fbo_A;
static GLuint fbo_B;
static GLuint fbo_A_tex;
static GLuint fbo_B_tex1;
static GLuint fbo_B_tex2;
static GLint BA_sampler_loc;
static GLint BB_sampler_loc;
static GLint imgB_sampler_loc;
static GLint img_Res_loc;

static GLuint buff_A_program;
static GLuint buff_B_program;
static GLuint img_program;

// uniforms
static GLint num_points_U;

static void fps() {
	static auto prev_time = std::chrono::steady_clock::now();
	static int counter = 0;
	auto now = std::chrono::steady_clock::now();
	counter++;
	if (now > prev_time + std::chrono::seconds(1)) {
		cout << "gfx fps: " << counter << endl;
		counter = 0;
		prev_time = now;
	}
}
static bool readShaderFile(filesys::path filepath, std::stringstream& shader_str) {
	return true;
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	// if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	// if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	// if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	// if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
}
static void window_size_callback(GLFWwindow* window, int width, int height) {
	window_width = width;
	window_height = height;

	// Resize textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbo_A_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, fbo_B_tex1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, fbo_B_tex2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
}
// I can get away with no void main(){} on my machine
static std::string VERT = R"(
#version 330
void main(){}
)";
static bool compile_shader(const GLchar* s, GLuint& sn, GLenum stype) {
	sn = glCreateShader(stype);
	glShaderSource(sn, 1, &s, NULL);
	glCompileShader(sn);
	GLint isCompiled = 0;
	glGetShaderiv(sn, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE) {
		GLint maxLength = 0;
		glGetShaderiv(sn, GL_INFO_LOG_LENGTH, &maxLength);
		vector<GLchar> errorLog(maxLength);
		glGetShaderInfoLog(sn, maxLength, &maxLength, &errorLog[0]);
		for (auto c : errorLog)
			cout << c;
		cout << endl;
		glDeleteShader(sn);
		return false;
	}
	return true;
}
static bool link_program(GLuint& pn, GLuint& vs, GLuint& gs, GLuint fs) {
	pn = glCreateProgram();
	glAttachShader(pn, gs);
	glAttachShader(pn, fs);
	glAttachShader(pn, vs);
	glLinkProgram(pn);
	GLint isLinked = 0;
	glGetProgramiv(pn, GL_LINK_STATUS, (int*)&isLinked);
	if (isLinked == GL_FALSE) {
		GLint maxLength = 0;
		glGetProgramiv(pn, GL_INFO_LOG_LENGTH, &maxLength);
		vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(pn, maxLength, &maxLength, &infoLog[0]);
		for (auto c : infoLog)
			cout << c;
		cout << endl;
		glDeleteProgram(pn);
		glDeleteShader(vs);
		glDeleteShader(fs);
		glDeleteShader(gs);
		return false;
	}
	return true;
}
static int frame_id = 0;
void draw(struct audio_data* audio) {
	//fps();
	static auto start_time = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	double elapsed = (now - start_time).count() / 1e9;

	// Update audio textures
	// glActivateTexture activates a certain texture unit.
	// each texture unit holds one texture of each dimension of texture
	//     {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBEMAP}
	// because I'm using four 1D textures I need to store them in separate texture units
	//
	// glUniform1i(textureLoc, int) sets what texture unit the sampler in the shader reads from
	//
	// A texture binding created with glBindTexture remains active until a different texture is
	// bound to the same target (in the active unit? I think), or until the bound texture is deleted
	// with glDeleteTextures. So I do not need to rebind
	// glBindTexture(GL_TEXTURE_1D, tex[X]);
	audio->mtx.lock();
	glActiveTexture(GL_TEXTURE0 + 0);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio->audio_l);
	glActiveTexture(GL_TEXTURE0 + 1);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio->audio_r);
	glActiveTexture(GL_TEXTURE0 + 2);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio->freq_l);
	glActiveTexture(GL_TEXTURE0 + 3);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio->freq_r);
	audio->mtx.unlock();






	// Render buffer A
	glUseProgram(buff_A_program);
	// Upload uniforms
	glUniform1f(num_points_U, float(POINTS));
	for (int i = 0; i < 4; ++i) // Point samplers to texture units
		glUniform1i(tex_loc[i], i);
	// Draw
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_A);
	glViewport(0, 0, window_width, window_height);
	glClearColor(0., 0., 0., 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_POINTS, 0, POINTS);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Which draw buffer and which uniform 



	// Render buffer B
	glUseProgram(buff_B_program);
	// Upload uniforms
	glUniform1i(BA_sampler_loc, 0); // Point samplers to texture units
	glUniform1i(BB_sampler_loc, 1 + (frame_id+1)%2); // Ping-pong
	// Draw
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_B);
	glDrawBuffer(GL_COLOR_ATTACHMENT0 + frame_id % 2); // Ping-pong
	glViewport(0, 0, window_width, window_height);
	glClearColor(0., 0., 0., 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_POINTS, 0, 1);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);




	// Render image
	glUseProgram(img_program);
	// Upload uniforms
	glUniform1i(imgB_sampler_loc, 1 + frame_id%2); // Point samplers to texture units
	glUniform2f(img_Res_loc, window_width, window_height);
	// Draw
	glClearColor(0., 0., 0., 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_POINTS, 0, 1);





	glfwSwapBuffers(window);
	glfwPollEvents();
	frame_id ++;
}

static bool compile_buffer_shaders(filesys::path shader_folder, std::string buff_name, GLuint& program) {
	cout << "Compiling shaders for buffer " + buff_name + "." << endl;
	filesys::path filepath;
	std::ifstream shader_file;
	std::stringstream geom_str;
	std::stringstream frag_str;

	filepath = filesys::path(shader_folder / (buff_name + ".geom"));
	if (! filesys::exists(filepath)) {
		cout << "Geometry shader does not exist." << endl;
		return false;
	}
	if (! filesys::is_regular_file(filepath)) {
		cout << buff_name+".geom is not a regular file." << endl;
		return false;
	}
	shader_file = std::ifstream(filepath);
	if (! shader_file.is_open()) {
		cout << "Error opening geometry shader." << endl;
		return false;
	}
	geom_str << shader_file.rdbuf();

	filepath = filesys::path(shader_folder / (buff_name + ".frag"));
	if (! filesys::exists(filepath)) {
		cout << "Fragment shader does not exist." << endl;
		return false;
	}
	if (! filesys::is_regular_file(filepath)) {
		cout << buff_name+".frag is not a regular file." << endl;
		return false;
	}
	if (shader_file.is_open())
		shader_file.close();
	shader_file = std::ifstream(filepath);
	if (! shader_file.is_open()) {
		cout << "Error opening fragment shader." << endl;
		return false;
	}
	frag_str << shader_file.rdbuf();

	GLuint vs, gs, fs;
	bool ok = compile_shader(VERT.c_str(), vs, GL_VERTEX_SHADER);
	if (!ok) {
		cout << "Internal error: vertex shader didn't compile." << endl;
		return ok;
	}
	ok = compile_shader(geom_str.str().c_str(), gs, GL_GEOMETRY_SHADER);
	if (!ok) {
		cout << "Failed to compile geometry shader." << endl;
		return false;
	}
	ok = compile_shader(frag_str.str().c_str(), fs, GL_FRAGMENT_SHADER);
	if (!ok) {
		cout << "Failed to compile fragment shader." << endl;
		return false;
	}
	ok = link_program(program, vs, gs, fs);
	if (!ok) {
		cout << "Failed to link program." << endl;
		return false;
	}

	cout << "Successfully compiled shader program for buffer " + buff_name + "." << endl;
	return true;
}

bool initialize_gl(std::string shader_folder_str) {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_DECORATED, false);
	window = glfwCreateWindow(window_width, window_height, "Music Visualizer", NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);

	glewExperimental = GL_TRUE;
	glewInit();
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported "<< version << endl;

	glfwSwapInterval(0);
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &max_output_vertices);
	glDisable(GL_DEPTH_TEST);

	// This isn't used for anything
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// I chose the following blending func because it allows the user to completely
	// replace the colors in the buffer by setting their output alpha to 1.
	// unfortunately it forces the user to make one of three choices:
	// 1) replace color in the framebuffer
	// 2) leave framebuffer unchanged
	// 3) mix new color with old color
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // mix(old_color.rgb, new_color.rgb, new_color_alpha)

	auto asdf = std::chrono::steady_clock::now();

	filesys::path shader_folder = filesys::path(shader_folder_str);
	bool ok = filesys::is_directory(shader_folder);
	if (!ok) {
		cout << "Shader folder path incorrect." << endl;
		return ok;
	}

	// Buff_A
	ok = compile_buffer_shaders(shader_folder, std::string("A"), buff_A_program);
	// Buff_B
	ok &= compile_buffer_shaders(shader_folder, std::string("B"), buff_B_program);
	// Image
	ok &= compile_buffer_shaders(shader_folder, std::string("image"), img_program);
	if (!ok) {
		return ok;
	}

	num_points_U = glGetUniformLocation(buff_A_program, "num_points");
	BA_sampler_loc = glGetUniformLocation(buff_B_program, "buff_A");
	BB_sampler_loc = glGetUniformLocation(buff_B_program, "buff_B");
	imgB_sampler_loc = glGetUniformLocation(img_program, "buff_B");
	img_Res_loc = glGetUniformLocation(img_program, "Res");

	// no window initial size uniform
	// window size uniform
	// buffer size uniform

	glGenTextures(4, tex);

#define ILOVEMACRO(X, Y)                                                                           \
	tex_loc[X] = glGetUniformLocation(buff_A_program, Y);                                          \
	glUniform1i(tex_loc[X], X);                                                                    \
	glActiveTexture(GL_TEXTURE0 + X);                                                              \
	glBindTexture(GL_TEXTURE_1D, tex[X]);                                                          \
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, VISUALIZER_BUFSIZE, 0, GL_RED, GL_FLOAT, NULL);        \
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);                                  \
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);                                  \
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);                              \
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	ILOVEMACRO(0, "SL")
	ILOVEMACRO(1, "SR")
	ILOVEMACRO(2, "FL")
	ILOVEMACRO(3, "FR")
#undef ILOVEMACRO

    // Setup Buff A
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &fbo_A_tex);
	glBindTexture(GL_TEXTURE_2D, fbo_A_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &fbo_A);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_A);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_A_tex, 0);

	// Setup Buff B
	glActiveTexture(GL_TEXTURE0 + 1);
	glGenTextures(1, &fbo_B_tex1);
	glBindTexture(GL_TEXTURE_2D, fbo_B_tex1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE0 + 2);
	glGenTextures(1, &fbo_B_tex2);
	glBindTexture(GL_TEXTURE_2D, fbo_B_tex2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &fbo_B);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_B);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_B_tex1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, fbo_B_tex2, 0);

	auto diff = std::chrono::steady_clock::now() - asdf;
	cout << diff.count()/1e9 << endl;
	return ok;
}

bool should_loop() {
	return !glfwWindowShouldClose(window);
}

void deinit_drawing() {
	glfwDestroyWindow(window);
}
