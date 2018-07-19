# About
Welcome to the project page for my music visualizer :)

Here is a comparison between what my app looks like and what other music
visualizers might look like when visualizing the same sound.

![](/docs/anim.gif)

# Examples

<img width="400" height="200" src="example0.PNG"> <img width="400" height="200" src="example3.png"> <img width="400" height="200" src="example6.PNG">
<img width="400" height="200" src="example1.png"> <img width="400" height="200" src="example4.png"> <img width="400" height="200" src="example7.PNG">
<img width="400" height="200" src="example2.png"> <img width="400" height="200" src="example5.png"> <img width="400" height="200" src="example8.PNG">

# Usage

The program is meant to feel like using shadertoy. What this means is that the user writes a .frag file that renders to a fullscreen quad (actually a window sized quad). If the user wants multipass buffers, then multiple .frag files should be written. The name of a buffer is the file name of the frag file without the .frag extension. The contents of a buffer are available in all buffer shaders as i[buffer name here]. So if the buffers A.frag and a B.frag exist, then B can access the contents of A by doing texture(iA, pos);.

Every shader must contain an image.frag file, just like shadertoy.

Code for a shader should be located in a folder named shaders that is in the same directory as the executable. Subdirectories of shaders/ can also contain code but that code will not be considered a part of the currently rendered shader.

A shadertoy like shader might have the following folder layout

	shader_viewer.exe
	shaders/
		image.frag
		buffA.frag
		buffB.frag

See [here](/docs/advanced.md) for details on how to configure the rendering process.

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
<a href="https://github.com/catchorg/Catch2">Catch2</a>
	Convenient testing framework<br>
