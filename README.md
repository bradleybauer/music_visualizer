Hi.

```
git clone --recursive https://github.com/xdaimon/music_visualizer.git
cd music_visualizer/
mkdir build
cd build
cmake ..
make -j4
./main
```

Depends on opengl >= 3.3, and pulseaudio.

Uses code from the following repositories
<a href="https://github.com/linkotec/ffts">ffts (linkotec fork)</a>
	fast fft library

<a href="https://github.com/karlstav/cava">cava</a>
	pulseaudio setup code

<a href="https://github.com/kritzikratzi/Oscilloscope">Oscilloscope</a>
	shader code for drawing smooth lines

Checkout the 'Feature Toggles' fold in pulse.cpp!
