include "vec.lobster"

fatal(gl_window("Shooter Tutorial", 640, 480))

while gl_frame() and gl_button("escape") != 1:
    gl_clear([ 0.0, 0.0, 0.0, 1.0 ])


