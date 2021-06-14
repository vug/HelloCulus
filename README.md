# HelloCulus

This is a small VR experience demo that renders GLSL raymarching fragment shaders in Oculus Rift VR headset. The GLSL file can be edited and reloaded while the app is running!

Example run: `HelloCulus.exe berry.glsl`

![helloculus_berry_example](https://user-images.githubusercontent.com/6636020/121844403-f2388480-ccb1-11eb-8684-fc0a49a861a1.gif)

Made with
* [Oculus SDK for Windows \| Developer Center \| Oculus](https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/)
* [freeglut](http://freeglut.sourceforge.net/) for starting an OpenGL context, creating a Window, and listening to key presses. (Looks like this is outdated. Will use GLFW next time.)
* [Glad](https://glad.dav1d.de/) for OpenGL extension function

I remember that shadertoy.com had this ability. But looks like some VR capabilities has been removed from browsers :-O

# Idea

* Read [Ray Marching and Signed Distance Functions](http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/) for the idea behind Ray Marching. 
It's a technique to render complex 3D visuals without using any 3D triangular mesh.
Geometries are determined via signed distance functions, i.e. implicit equations whose value is zero at the 3D surface they define. 
Simplest one being a sphere at `c` of radius `r`: `sdfSphere(p) = length(p - c) - r`.
This function of `p` has values `0` when `p` is on the sphere's surface.
* For each pixel a ray is casted and marched with some optimization until it finds a point whose value is 0. Then, at the point the color value is computed (based on the surface normal (again computed via SDFs), light location, material computation etc)
* To combine raymarching with VR, a GLSL shader is given the eye coordinate and the head orientation, via uniforms, using which casted ray origins (`ro`) and directions (`rd`) are set accordingly.
* This technique does not require any geometry. So, no vertex shaders are used! 
For geometry, only a quad is drawn from (-1, -1) to (1, 1) that covers whole screen.

# Usage

* Have following uniforms in GLSL fragment shader
  * `float time`: to be able to animate things.
  * `vec3 ro`: "ray origin" for each eye its position will be given to the shader
  * `mat4 view, proj`: view and projection matrices will be given to the shader just in case
  * `float frustFovH, frustFovV`: "frustrum fov angles" 
  * `int eyeNo`: will be 0 for left eye and 1 for right eye
  * `float param1`: generic parameter that can be used for any reason
* In raymarching algorithm use `ro` as the ray origin, and use following logic as ray direction `rd`.
```
vec2 res = vec2(1344, 1600); // my HMD resolution per eye
vec2 uv = (1.0 * gl_FragCoord.xy - 0.5 * res) / res;  // normalize texture coordinates to [-1, 1] range
vec3 forward = normalize(view * vec4(0, 0, -1, 1)).xyz; // local forward/backward direction
vec3 u = normalize(cross(vec3(0, 1, 0), forward)); // local left/right direction
vec3 v = normalize(cross(forward, u)); // local up/down direction
vec3 rd = normalize(-uv.x * u + uv.y * v + zoom * forward); // zoom = 1.5
```
* Write the rest as a standard ray-marching shader
* run `HelloCulus.exe MY_SHADER.glsl`
* can edit the GLSL file and press `G` to reload the shader.
  * If fails compilation look at the console to see errors.
  * If compiles successfuly the rendering will be updated without restarting the app
* Move around in the 3D world wrt to local orientation
  * `W`, `S`: forward, backward
  * `A`, `D`: left, right
  * `Q`, `E`: up, down
* Turn around in the 3D world
  * `4`, `5`: turn 22.5 degrees left/right on local horizontal direction
  * Asking a user to turn around all the time is not a nice experience, these discrete jumps make navigation more comfortable

# Issues

* I'm sure ray-direction logic can be improved. 
  * i.e. probably I could have computed local coordinates in C++ and give them to shader via uniforms.
  * maybe a logic that utilize `frustFovH` and `frustFovV` would be better. Solving `rd` was the main problem of bringing a raymarching shader to VR.
* The virtual head location sometimes jumps around randomly. Either my sensors are not placed well, or I need further complex logic to bring stability.
* Need to study how to set the initial location of virtual eye and sizes of virtual objects. 
  * Experienced that when objects are too small it feels like a cross-eyed view. 
  * i.e. the render on the left eye is too far away from the render of the right eye, which breaks the virtual/3D illusion.
  * Did some "hack" by manipulating eye-to-eye (Inter-Pupil-Distance(?)) distance :-) added a parameter to move them closer or further away. But a proper solution is needed.
* Further abstractions in GLSL code would be good. 
  * Provide common GLSL functionality that'll be included with every shader (Perlin Noise, hash noise, raymarching logic, `ro` and `rd` computation, material logic, shadow logic, ...)
  * Let the artist just write the juicy part without boiler plate
* Even though, the app can reload shaders, editing them while the app is running is not as convenient. Need to remove headset, edit file on computer, reload, put the headset back.
  * Instead, having an editor that overlays on top of the rendered visuals in VR could be a better solution. (There are other GLSL renderes with that ability)
  * Say, press a button to open the text editor, which should be rendere using a `Quad` Layer type. User edits the file while keeping the headset. Then reload and hide with different buttons.
