Welcome to the project page for my music visualizer :)

Here is a comparison between what my app looks like and what other music
visualizers might look like when visualizing the same sound.

![](anim.gif)

On Ubuntu
```
TODO apt install for required libs and headers
git clone --recursive https://github.com/xdaimon/music_visualizer.git
cd music_visualizer
mkdir build; cd build; cmake ..; make -j4; ./main
```
On Windows 10 machine with the fall creators update: build the x64 Release solution.

Depends on opengl >= 4.3

Uses code from the following repositories<br>
<a href="https://github.com/linkotec/ffts">ffts</a>
	Fast fft library<br>
<a href="https://github.com/karlstav/cava">cava</a>
	Pulseaudio setup code<br>
<a href="https://github.com/kritzikratzi/Oscilloscope">Oscilloscope</a>
	Shader code for drawing smooth lines<br>
<a href="https://github.com/shadowndacorner/SimpleFileWatcher">SimpleFileWatcher</a>
	Async recursive file watcher<br>
<a href="https://github.com/rapidjson/rapidjson">RapidJson</a>
	Fast json file reader<br>

Here is a youtube video of the app displaying a waveform.<br>
<a href="https://youtu.be/yxGM7H1RFRA">https://youtu.be/yxGM7H1RFRA</a>

![](example1.png)
![](example2.png)
![](example3.png)
![](example4.png)
![](example5.png)
