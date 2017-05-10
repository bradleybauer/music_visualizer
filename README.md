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

Uses code from the following repositories:


	ffts (linkotec fork): 
		[a link](https://github.com/linkotec/ffts)
		fast fft

	cava: 
		[a link](https://github.com/karlstav/cava)
		pulseaudio setup code

	Oscilloscope: 
		[a link](https://github.com/kritzikratzi/Oscilloscope)
		shader code for drawing smooth lines

Checkout the 'Feature Toggles' fold in pulse.cpp!
