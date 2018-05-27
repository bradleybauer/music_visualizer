# About
Welcome to the project page for my music visualizer :)

Here is a comparison between what my app looks like and what other music
visualizers might look like when visualizing the same sound.

![](anim.gif)

# Usage

Warning - this section describes unimplemented features (shadertoy like default behavior).

To visualize sound that plays to the system default audio output

	Turn on music -> Run the app

If you want to dig into the app's shader code or write your own shaders then read on.

I intend to provide a shadertoy like experience by default. What this means is that the user writes a .frag file that renders to a fullscreen quad (actually a window sized quad). If the user wants multipass buffers, then multiple .frag files should be written. The name of a buffer is the file name of the frag file without the .frag extension. The contents of a buffer are available in all buffer shaders as i[buffer name here]. So if the buffers A.frag and a B.frag exist, then B can access the contents of A by doing texture(iA, pos);.

Every shader must contain an image.frag file, just like shadertoy.

Code for a shader should be located in a folder named shaders that is in the same directory as the executable. Subdirectories of shaders/ can also contain code but that code will not be considered a part of the currently rendered shader.

A shadertoy like shader might expect the following folder layout

	executable
	shaders/
		image.frag
		buffA.frag
		buffB.frag

If the user modifies the current shader's files while the app is running, then the app will load the changes, recompile the shaders, and render the new shaders if everything compiled correctly.

More advanced usage is supported by giving the user access to geometry shaders. The user could render a glittery sphere, for example, using only geometry shaders and a very simple frag shader. To do this the user would write a geometry shader, say buffname.geom, which would be executed a number of times and on each execution would output a triangle or quad to be shaded by a buffname.frag. The geometry shader knows which execution it is currently on and so can decide where to place the output geometry (and how to apply a perspective transform) so that a sphere is generated.

The size of each framebuffer can also be configured so that unnecessary compute can be avoided. For example, shader games could be implemented where there is a state buffer and a separate rendering buffer. The state framebuffer could be of size 2x100, if for instance the user is simulating a hundred 2D balls moving around. The geometry shader would execute once and draw a full buffer quad. The fragment shader would then shade each pixel in this quad where a pixel is one of the two coordinates for one of the one hundred 2D balls.. Another approach would have the geometry shader output two one-pixel sized quads (or triangles?). The geometry shader could do all the work of updating each ball's state and pass the new state to an 'assignment' fragment shader to be written into the framebuffer. In this case the geometry shader would execute 100 times.

A shader using geometry shaders might expect this folder layout

	executable
	shaders/
		image.frag
		buffA.frag
		buffA.geom
		shader.json

# Configuration

If you provide .geom shaders or want to change certain options, then you should have a shader.json file in shaders/.

Here is a list of available options that can be set in shader.json:

	initial window size

	image buffer geometry iterations
	image buffer clear color

	buffer name
	buffer size
	buffer geometry iterations
	buffer clear color

	render order of buffers

	whether to blend geometry drawn into a framebuffer

	whether the audio system is enabled
	whether the audio fft sync is enabled
	whether the audio diff sync is enabled
	whether the fft output is smoothed
	whether the waveform is smoothed

	a list of values available in all shaders as uniforms

if no shader.json is provided, then the default values are assumed.

See [here](../src/shaders/oscilloscope/shader.json) for more detail.

# Default behavior

Not implemented yet

	if there is no shader.json then ignore all geometry files (provide warning if there are geometry files)

	if there is no buffname.geom for a buffname.frag file, then buffname.frag draws a fullscreen (window sized) quad

	if there is a shader.json with buffer options (size or geom_iters) set for buffname but there is no buffname.geom then take default options for size and geom_iters (provide warning)

	if there is a shader.json then
	  if no buffname.geom_iters -> buffname.geom_iters = 1
	  if no buffname.buffer_size -> buffname.buffer_size = "window_size", buffname.geom_iters = 1 (overrides user's geom_iters)

	...

See [here](../src/shaders/oscilloscope/shader.json) for more detail.

# Building

First get the sources
```
git clone --recursive https://github.com/xdaimon/music_visualizer.git
```
Then to build on Ubuntu
```
sudo apt install cmake libglfw3-dev libglew-dev libpulse-dev
cd music_visualizer
mkdir build; cd build; cmake ..; make -j4
```

and on Windows 10 with Visual Studio 2017:
```
build the x64 Release configuration
```

# TODO

buffers should have GL_LINEAR/GL_NEAREST and GL_CLAMP/GL_REPEAT options

Would it be worthwhile to print current settings to the terminal window?

# Thanks To

<a href="https://github.com/linkotec/ffts">ffts</a>
	Fast fft library<br>
<a href="https://github.com/karlstav/cava">cava</a>
	Pulseaudio setup code<br>
<a href="https://github.com/kritzikratzi/Oscilloscope">Oscilloscope</a>
	Shader code for drawing smooth lines<br>
<a href="https://github.com/shadowndacorner/SimpleFileWatcher">SimpleFileWatcher</a>
	Asyncronous recursive file watcher<br>
<a href="https://github.com/rapidjson/rapidjson">RapidJson</a>
	Fast json file reader<br>

# Examples

Here is a youtube video of the app displaying a waveform.<br>
<a href="https://youtu.be/yxGM7H1RFRA">https://youtu.be/yxGM7H1RFRA</a>

![](example0.PNG)
![](example3.png)
![](example1.png)
![](example2.png)
![](example4.png)
![](example5.png)
