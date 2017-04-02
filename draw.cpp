/* IDEA
 * Draw lissajous figures using data from the fft
 */
/* https://open.gl/geometry
 * https://www.khronos.org/opengl/wiki/Geometry_Shader
 * http://www.informit.com/articles/article.aspx?p=2120983&seqNum=2
 */
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
using std::cout;
using std::endl;

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "draw.h"

static GLFWwindow* window;

#include "audio_data.h"
static const int POINTS = BUFFSIZE-1; // audio buffer size - 1
// static const int POINTS = 1;
static int maxOutputVertices;
static const int COORDS_PER_POINT = 1;
static int pointIndices[POINTS];

static int windowWidth = 400;
static int windowHeight = 400;

static GLint numPointsUniform;
static GLint maxOutputVerticesUniform;
static GLint resolutionUniform;
static GLint timeUniform;

static GLuint vbo;
static GLuint vao;
static GLuint tex[4];
static GLuint tex_loc[4];
static GLuint program;

static void fps() {
	static auto prev_time = std::chrono::steady_clock::now();
    static int counter = 0;
	auto now = std::chrono::steady_clock::now();
	counter++;
    if (now > prev_time + std::chrono::seconds(1)) {
		cout << "FPS: " << counter << endl;
		counter = 0;
		prev_time = now;
    }
}

void log_gl_error() {
    const GLubyte* err = gluErrorString(glGetError());
    while (*err) {
		cout << *err;
		err++;
    }
    cout << endl;
}

long filelength(FILE *file) {
    long numbytes;
    long savedpos = ftell(file);
    fseek(file, 0, SEEK_END);
    numbytes = ftell(file);
    fseek(file, savedpos, SEEK_SET);
    return numbytes;
}

unsigned char *readShaderFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
		cout << "ERROR Cannot open shader file!" << endl;
		return 0;
    }
    int bytesinfile = filelength(file);
    unsigned char *buffer = (unsigned char *)malloc(bytesinfile + 1);
    int bytesread = fread(buffer, 1, bytesinfile, file);
    buffer[bytesread] = 0; // Terminate the string with 0
    fclose(file);

    return buffer;
}

bool compile_shaders() {
    const GLchar* vertex_shader = (GLchar*)readShaderFile("vertex.glsl");
    const GLchar* fragment_shader = (GLchar*)readShaderFile("image.glsl");
    const GLchar* geometry_shader = (GLchar*)readShaderFile("geom.glsl");

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, NULL);
    glCompileShader(vs);

    GLint isCompiled = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
	    GLint maxLength = 0;
	    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);
     
	    // The maxLength includes the NULL character
	    std::vector<GLchar> errorLog(maxLength);
	    glGetShaderInfoLog(vs, maxLength, &maxLength, &errorLog[0]);
     
	    for (auto c : errorLog)
			cout << c;

	    glDeleteShader(vs); // Don't leak the shader.
	    return false;
    }


    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    isCompiled = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
	    GLint maxLength = 0;
	    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);
     
	    // The maxLength includes the NULL character
	    std::vector<GLchar> errorLog(maxLength);
	    glGetShaderInfoLog(fs, maxLength, &maxLength, &errorLog[0]);
     
	    for (auto c : errorLog)
			cout << c;

	    glDeleteShader(fs); // Don't leak the shader.
	    return false;
    }

    GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(gs, 1, &geometry_shader, NULL);
    glCompileShader(gs);

    isCompiled = 0;
    glGetShaderiv(gs, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
	    GLint maxLength = 0;
	    glGetShaderiv(gs, GL_INFO_LOG_LENGTH, &maxLength);
     
	    // The maxLength includes the NULL character
	    std::vector<GLchar> errorLog(maxLength);
	    glGetShaderInfoLog(gs, maxLength, &maxLength, &errorLog[0]);
     
	    for (auto c : errorLog)
			cout << c;

	    glDeleteShader(gs); // Don't leak the shader.
	    return false;
    }

    program = glCreateProgram();
    glAttachShader(program, gs);
    glAttachShader(program, fs);
    glAttachShader(program, vs);
    glLinkProgram(program);
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if(isLinked == GL_FALSE)
    {
	    GLint maxLength = 0;
	    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
     
	    //The maxLength includes the NULL character
	    std::vector<GLchar> infoLog(maxLength);
	    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
     
	    for (auto c : infoLog)
			cout << c;

	    //We don't need the program anymore.
	    glDeleteProgram(program);
	    //Don't leak shaders either.
	    glDeleteShader(vs);
	    glDeleteShader(fs);
	    glDeleteShader(gs);
     
	    return false;
    }

    return true;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    // if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	// toggle_play();
}
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    // if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    //
    // if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    //
    // if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    //
    // if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
}
static void window_size_callback(GLFWwindow* window, int width, int height) {
	windowWidth = width;
	windowHeight = height;
    glViewport(0, 0, width, height);
}
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
}

void init_desktop() {
    glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfwWindowHint(GLFW_ALPHA_BITS, 8);
	// glfwWindowHint(GLFW_RED_BITS, 8);
	// glfwWindowHint(GLFW_GREEN_BITS, 8);
	// glfwWindowHint(GLFW_BLUE_BITS, 8);
	// glfwWindowHint(GLFW_ACCUM_ALPHA_BITS, 8);
	// glfwWindowHint(GLFW_ACCUM_RED_BITS, 8);
	// glfwWindowHint(GLFW_ACCUM_GREEN_BITS, 8);
	// glfwWindowHint(GLFW_ACCUM_BLUE_BITS, 8);

	// glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(windowWidth, windowHeight, "float me", NULL, NULL);

    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    glewInit();

    // const GLubyte* renderer = glGetString(GL_RENDERER);
    // const GLubyte* version = glGetString(GL_VERSION);
    // const GLubyte* ext = glGetString(GL_EXTENSIONS);
    // cout << "Renderer: " << renderer << endl;
    // cout << "OpenGL version supported "<< version << endl;
    // cout << "OpenGL extensions supported "<< ext << endl;

    glViewport(0, 0, 420, 420);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}

bool init_render() {
	const bool ret = compile_shaders();

	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &maxOutputVertices);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // regular?
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // for nice lines?
    glClearColor(0., 0., 0., 1.);

	for (int i = 0; i < POINTS; ++i)
		pointIndices[i] = i;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, POINTS * sizeof(int), pointIndices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

	const GLuint loc = glGetAttribLocation(program, "_pointIndex");
    glEnableVertexAttribArray(loc);
    glVertexAttribIPointer(loc, COORDS_PER_POINT, GL_INT, 0, NULL);
	// glDisableVertexAttribArray(loc);

	numPointsUniform = glGetUniformLocation(program, "numPoints");
	maxOutputVerticesUniform = glGetUniformLocation(program, "maxOutputVertices");
	resolutionUniform = glGetUniformLocation(program, "R");
	timeUniform = glGetUniformLocation(program, "T");

	glGenTextures(4, tex);

    #define ILOVEMACRO \
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);     \
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);     \
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); \
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, tex[0]);
	tex_loc[0] = glGetUniformLocation(program, "SL"); 
	glUniform1i(tex_loc[0], 0);
	ILOVEMACRO
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, tex[1]);
	tex_loc[1] = glGetUniformLocation(program, "SR"); 
	glUniform1i(tex_loc[1], 1);
	ILOVEMACRO
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, tex[2]);
	tex_loc[2] = glGetUniformLocation(program, "FL"); 
	glUniform1i(tex_loc[2], 2);
	ILOVEMACRO
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_1D, tex[3]);
	tex_loc[3] = glGetUniformLocation(program, "FR"); 
	glUniform1i(tex_loc[3], 3);
	ILOVEMACRO

	return ret;
}

bool initialize_gl() {
    init_desktop();
	return init_render();
}

void draw(struct audio_data* audio) {
	fps();
	static auto start_time = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	double elapsed = (now - start_time).count()/1e9;
	// cout << elapsed << endl;

	// static auto prev_time = std::chrono::steady_clock::now();
	// auto delta = now - prev_time;
	// prev_time = now;
	// cout << delta.count()/1e9 << endl;

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// glUniform1i(numPointsUniform, BUFFSIZE-1);
	   glUniform1i(numPointsUniform, POINTS);
	glUniform1i(maxOutputVerticesUniform, maxOutputVertices);
	glUniform2f(resolutionUniform, float(windowWidth), float(windowHeight));
	glUniform1f(timeUniform, elapsed);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, tex[0]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, BUFFSIZE, 0, GL_RED, GL_FLOAT, audio->audio_l);
	glUniform1i(tex_loc[0], 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, tex[1]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, BUFFSIZE, 0, GL_RED, GL_FLOAT, audio->audio_r);
	glUniform1i(tex_loc[1], 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, tex[2]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, BUFFSIZE, 0, GL_RED, GL_FLOAT, audio->freq_l);
	glUniform1i(tex_loc[2], 2);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_1D, tex[3]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, BUFFSIZE, 0, GL_RED, GL_FLOAT, audio->freq_r);
	glUniform1i(tex_loc[3], 3);

    glDrawArrays(GL_POINTS, 0, POINTS);
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool should_loop() {
    return !glfwWindowShouldClose(window);
}

void deinit_drawing() {
    glfwDestroyWindow(window);
}

