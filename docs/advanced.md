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

If options in shader.json are not given, then for most options the default values are assumed.

Here is a shader.json with all the options described:

    {
        // If you want to specify options in "image", "buffers", "render_order", "blend", then set "shader_mode" to "advanced".
        // Otherwise default values are assumed and buffers are created for any .frag file in shaders/
        // Defaults to "easy"
        "shader_mode":"advanced",

        // initial window size
        // Defaults to [400,300]
        "initial_window_size":[500,250],

        // image buffer is always the size of the window
        "image": {
            // Defaults to 1
            "geom_iters":1,

            // Defaults to [0,0,0]
            "clear_color":[0,0,0]
        },

        // In addition to drawing an image buffer you can define other buffers to draw here
        // Available as iBuffName in all shaders.
        "buffers": {
            "A": {
                // Defaults to "window_size"
                "size": "window_size",

                // How many times the geometry shader will execute
                "geom_iters":1024,

                // RGB values from the interval [0, 1]
                // Defaults to [0,0,0]
                "clear_color":[0, 0, 0]
            },
            "B": {
                "size": [100,3],
                "geom_iters":1,
                "clear_color":[0, 0, 0]
            }
        },

        // Whether glBlend is enabled
        // Defaults to false
        "blend":true,

        // Render A then B and then B again
        // Every buffer has access to the most recent output of all buffers except image
        // Defaults to the order of the buffers in "buffers"
        "render_order":["A", "B", "B"],

        // Defaults to true
        "audio_enabled":true,
        "audio_options": {

            // Defaults to true
            "fft_sync":true,

            // whether the cross correlation sync is enabled
            // Defaults to true
            "xcorr_sync":true,

            // how much to mix the prev fft buff with the new fft buff
            // Defaults to 1
            "fft_smooth":0.5, // not implemented yet

            // how much to mix the prev wave buff with the new wave buff
            // Defaults to 0.8
            "wave_smooth":0.8
        },

        // TODO just write these in as const variables into the shader?
        // Useful for setting colors from external scripts.
        // Available as UniformName in all buffers.
        "uniforms": {
            "my_uni": [10, 123, 42],
            "your_uni":[25, 20, 1],
            "his_uni":[1.0, 2.0, 3.0, 4]
        }
    }

