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
static const int COORDS_PER_POINT = 1;
static int point_indices[POINTS];

// window dimensions
static int window_width = 1000;
static int window_height = 400;

static GLuint vbo;       // vertex buffer object
static GLuint vao;       // vertex array object
static GLuint tex[4];    // textures
static GLint tex_loc[4]; // texture uniform locations
static GLuint fb;        // frameuffer
static GLuint fbtex;     // framebuffer texture
static GLint fbtex_loc;  // framebuffer texture location in display program
static GLuint prev_pixels;
static GLint prev_pixels_loc;
static GLubyte* last_pixels;
static int last_pixels_size = 4 * 1920 * 1080;

static GLuint buf_program;
static GLuint img_program;

// uniforms
static GLint num_points_U;
static GLint resolution_img_U;
static GLint resolution_buf_U;
static GLint time_U;

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
static long filelength(FILE* file) {
	long numbytes;
	long savedpos = ftell(file);
	fseek(file, 0, SEEK_END);
	numbytes = ftell(file);
	fseek(file, savedpos, SEEK_SET);
	return numbytes;
}
// TODO USE C++ FILESYSTEM FEATURES
static bool readShaderFile(const char* filename, GLchar*& out) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		cout << "ERROR Cannot open shader file!" << endl;
		return false;
	}
	int bytesinfile = filelength(file);
	out = (GLchar*)malloc(bytesinfile + 1);
	int bytesread = fread(out, 1, bytesinfile, file);
	out[bytesread] = 0; // Terminate the string with 0
	fclose(file);
	return true;
}

// Vim tip ']p' -> reindented paste

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
	glViewport(0, 0, window_width, window_height);
	if (4 * window_height * window_width > last_pixels_size) {
		// Resize buffer
		last_pixels_size = 4 * window_height * window_width;
		last_pixels = (GLubyte*)realloc(last_pixels, last_pixels_size * sizeof(GLubyte));
	}

	// Resize framebuffer texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbtex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	// Resize prev_pixels texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, prev_pixels);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	memset(last_pixels, 255, last_pixels_size * sizeof(GLubyte));
}
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
}
static std::string VERT = R"(
#version 330
void main(){}
)";
static std::string GEOM = R"(
#version 330
precision highp float;
layout(points) in;
layout(triangle_strip, max_vertices = 6) out;
out vec2 p;
void main() {
	/* 1------3
	   | \    |
	   |   \  |
	   |     \|
	   0------2 */
	const vec2 p0 = vec2(-1., -1.);
	const vec2 p1 = vec2(-1., 1.);
	gl_Position = vec4(p0, 0., 1.);
	p = p0 * .5 + .5;
	EmitVertex(); // 0
	gl_Position = vec4(p1, 0., 1.);
	p = p1 * .5 + .5;
	EmitVertex(); // 1
	gl_Position = vec4(-p1, 0., 1.);
	p = -p1 * .5 + .5;
	EmitVertex(); // 2
	EndPrimitive();

	gl_Position = vec4(-p1, 0., 1.);
	p = -p1 * .5 + .5;
	EmitVertex(); // 2
	gl_Position = vec4(p1, 0., 1.);
	p = p1 * .5 + .5;
	EmitVertex(); // 1
	gl_Position = vec4(-p0, 0., 1.);
	p = -p0 * .5 + .5;
	EmitVertex(); // 3
	EndPrimitive();
}
)";
static std::string FRAG = R"(
#version 330
precision highp float;
uniform sampler2D t0; // new buffer
uniform sampler2D t1; // previous buffer
uniform vec2 Res;
in vec2 p;
out vec4 c;
//vec4 bg=vec4(0.);
//vec4 fg=vec4(0.,204./255.,1.,1.);
//
//vec4 bg = vec4(1.);
//vec4 fg = vec4(0., 204. / 255., 1., 1.);
//
// vec4 bg=vec4(1.);
// vec4 fg=vec4(204./255.,0.,.1,1.);
//
// vec4 bg=vec4(0.);
// vec4 fg=vec4(1.,1.,.1,1.);
//
// vec4 bg=vec4(0);
// vec4 fg=vec4(1);
//
vec4 bg=vec4(1);
vec4 fg=vec4(0);
//
const float MIX = .8;
const float bright = 7.;
void main() {
	// ASPECT RATIO ADJUSTED
		// vec2 U = gl_FragCoord.xy / Res;
		// U = U * 2. - 1.;
		// U.x *= max(1., Res.x / Res.y);
		// U.x = clamp(U.x, -1., 1.);
		// U.y *= max(1., Res.y / Res.x);
		// U.y = clamp(U.y, -1., 1.);
		// U = U * .5 + .5;
		// if (U.x == 1. || U.x == 0.)
		// 	c = bg;
		// else if (U.y == 1. || U.y == 0.)
		// 	c = bg;
		// else
		// 	c = mix(bg, fg, bright*texture(t0, U).r);
		// c = mix(c, texture(t1, p), MIX);

	// NOT ASPECT RATIO ADJUSTED
	float new_intensity = texture(t0, p).r;
	vec4 old_color = texture(t1, p);
	vec4 new_color = mix(bg, fg, bright * new_intensity);
	c = mix(new_color, old_color, MIX);

	// allow black background instead of just very dark grey
	// c-=.002;

	// fade out near sides of window
	// c = mix(bg, c, smoothstep(1., .8, abs(p.x * 2. - 1.)));
}
)";
static bool compile_shader(GLchar* s, GLuint& sn, GLenum stype) {
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
void draw(struct audio_data* audio) {
	fps();
	static auto start_time = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	double elapsed = (now - start_time).count() / 1e9;

	// Render to texture
	glUseProgram(buf_program);

	glUniform1f(num_points_U, float(POINTS));
	glUniform2f(resolution_img_U, float(window_width), float(window_height));
	glUniform1f(time_U, elapsed);

	glBindFramebuffer(GL_FRAMEBUFFER, fb);
	glClear(GL_COLOR_BUFFER_BIT);

// glActivateTexture activates a certain texture unit.
// each texture unit holds one texture of each dimension of texture
//     {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBEMAP}
// because I'm using four 1 dimensional textures I need to store them in separate texture units
//
// glUniform1i(textureLoc, int) sets what texture unit the sampler in the shader reads from
//
// A texture binding created with glBindTexture remains active until a different texture is
// bound to the same target (in the active unit? I think), or until the bound texture is deleted
// with glDeleteTextures. So I do not need to rebind
// glBindTexture(GL_TEXTURE_1D, tex[X]);
#define TEXMACRO(X, Y)                                                                  \
	glActiveTexture(GL_TEXTURE0 + X);                                                     \
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio->Y); \
	glUniform1i(tex_loc[X], X);

	audio->mtx.lock();
	TEXMACRO(0, audio_l)
	TEXMACRO(1, audio_r)
	TEXMACRO(2, freq_l)
	TEXMACRO(3, freq_r)
	audio->mtx.unlock();
#undef TEXMACRO

	glDrawArrays(GL_POINTS, 0, POINTS);

	// Render to screen
	glUseProgram(img_program);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // bind window manager's fbo
	glClear(GL_COLOR_BUFFER_BIT);

	glUniform2f(resolution_buf_U, float(window_width), float(window_height));
	glUniform1i(fbtex_loc, 0);
	glUniform1i(prev_pixels_loc, 1);
	glActiveTexture(GL_TEXTURE1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, window_width, window_height, GL_RGBA, GL_UNSIGNED_BYTE,
	                last_pixels);

	glDrawArrays(GL_POINTS, 0, 1);

	glReadPixels(0, 0, window_width, window_height, GL_RGBA, GL_UNSIGNED_BYTE, last_pixels);

	glfwSwapBuffers(window);
	glfwPollEvents();
}

bool initialize_gl() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_DECORATED, false);
	window = glfwCreateWindow(window_width, window_height, "Music Visualizer", NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSwapInterval(0);
	glewExperimental = GL_TRUE;
	glewInit();
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported "<< version << endl;
	glViewport(0, 0, window_width, window_height);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);

	bool ret = true;
	GLchar *fs_c, *gs_c;
	ret = readShaderFile("../shaders/geom.glsl", gs_c);
	ret = readShaderFile("../shaders/image.glsl", fs_c);
	if (!ret) {
		ret = readShaderFile("shaders/geom.glsl", gs_c);
		ret = readShaderFile("shaders/image.glsl", fs_c);
		if (!ret) {
			cout << "Did not open any shader files." << endl;
			ret = false;
			return ret;
		}
	}
	GLuint vs, gs, fs;
	ret = compile_shader((GLchar*)VERT.data(), vs, GL_VERTEX_SHADER);
	ret = compile_shader(gs_c, gs, GL_GEOMETRY_SHADER);
	ret = compile_shader(fs_c, fs, GL_FRAGMENT_SHADER);
	ret = link_program(buf_program, vs, gs, fs);
	ret = compile_shader((GLchar*)GEOM.data(), gs, GL_GEOMETRY_SHADER);
	ret = compile_shader((GLchar*)FRAG.data(), fs, GL_FRAGMENT_SHADER);
	ret = link_program(img_program, vs, gs, fs);
	if (!ret) {
		cout << endl << endl << "Could not find shader file OR could not compile shaders." << endl << endl;
		return ret;
	}

	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &max_output_vertices);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE); // for nice lines?
	glClearColor(0., 0., 0., 1.);

	for (int i = 0; i < POINTS; ++i)
		point_indices[i] = i;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, POINTS * sizeof(int), point_indices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	const GLuint loc = glGetAttribLocation(buf_program, "_point_index");
	glEnableVertexAttribArray(loc);
	glVertexAttribIPointer(loc, COORDS_PER_POINT, GL_INT, 0, NULL);
	// glDisableVertexAttribArray(loc);

	num_points_U = glGetUniformLocation(buf_program, "num_points");
	resolution_img_U = glGetUniformLocation(buf_program, "Res");
	resolution_buf_U = glGetUniformLocation(img_program, "Res");
	time_U = glGetUniformLocation(buf_program, "Time");

	glGenTextures(4, tex);

#define ILOVEMACRO(X, Y)                                                                         \
	tex_loc[X] = glGetUniformLocation(buf_program, Y);                                             \
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

	glGenTextures(1, &fbtex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbtex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbtex, 0);

	glActiveTexture(GL_TEXTURE1);
	glGenTextures(1, &prev_pixels);
	glBindTexture(GL_TEXTURE_2D, prev_pixels);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// location of the sampler2D in the fragment shader
	fbtex_loc = glGetUniformLocation(img_program, "t0");
	prev_pixels_loc = glGetUniformLocation(img_program, "t1");

	last_pixels = (GLubyte*)malloc(sizeof(GLubyte)*last_pixels_size);

	return ret;
}

bool should_loop() {
	return !glfwWindowShouldClose(window);
}

void deinit_drawing() {
	glfwDestroyWindow(window);
}
