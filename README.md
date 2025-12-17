# Interactive Solar System Simulation (OpenGL + C++)

A **real-time, interactive Solar System simulation** built using **Modern OpenGL** and **C++**, featuring physically inspired planetary motion, advanced post-processing effects, geographic exploration, and a dynamic UI using **Dear ImGui**.

This project focuses on **graphics programming**, **shader pipelines**, and **real-time rendering techniques**, making it suitable for **computer graphics courses, OpenGL projects, and academic demos**.

---

## Project Overview

This simulation renders the **Sun, 8 planets, moons, asteroid belts, and a starfield**, allowing users to:

- Orbit and zoom around planets
- Focus on individual planets or moons
- Explore **geographic locations on Earth and Saturn**
- View real-time planet data via an ImGui dashboard
- Experience cinematic effects like **Bloom, God Rays, and HDR tone mapping**

All rendering is done using **custom GLSL shaders** with a **multi-pass framebuffer pipeline**.

---

## Key Features

### Solar System Simulation
- Sun at the center with **procedural animated surface**
- 8 planets with correct relative sizes and orbits
- Moons orbiting their parent planets
- Inner asteroid belt + Kuiper belt
- Elliptical orbits with dotted visualization

### Geographic Exploration
- **Earth locations**: Ocean, Mountain, Amazon, Desert
- **Saturn locations**: Diamond Mountain, Chloric Ocean
- Latitude/longitude mapped to spherical coordinates
- Click or key-based location selection
- Smooth camera transition to surface locations

### Advanced Rendering & Effects
- **Deferred-style multi-pass rendering**
- HDR framebuffer
- Bloom (Gaussian blur)
- God Rays (Light scattering from Sun)
- Tone mapping + gamma correction
- Transparent atmospheric layers (Earth clouds, Venus atmosphere)
- Sky sphere with animated star twinkling

### Minimap
- Real-time **top-down orthographic minimap**
- Displays entire solar system layout
- Appears when exploring geographic locations

### ImGui UI
- Planet information panel (size, rotation, atmosphere, moons)
- Interactive location selection panels
- Live simulation feedback
- Clean, readable UI for demos & viva

---

## 📁 Project Structure
```
OpenGLProject/
│
├── include/ # GLAD, GLFW, GLM headers
├── lib/ # glfw3.lib
├── src/
│ ├── solar.cpp # Main application
│ ├── glad.c
│ ├── imgui*.cpp/h
│ └── stb_image.h
│
├── textures/
│ ├── sun.bmp
│ ├── earth_daymap.bmp
│ ├── earth_clouds.bmp
│ ├── mars.bmp
│ ├── jupiter.bmp
│ ├── saturn.bmp
│ └── ...
│
├── imgui.ini
├── Solar.exe
└── README.md
```

---

## Controls
```
### Planet Focus
0 → Sun
1 → Mercury
2 → Venus
3 → Earth
4 → Mars
5 → Jupiter
6 → Saturn
7 → Uranus
8 → Neptune
9 -> moon earth
```

### Time Control
```
+ -> continuous clicking to show effects fast
- -> continuous clicks to show effects slow
```

### Camera
```
- Left mouse drag → Rotate camera
- Scroll → Zoom in/out
```
<h2>compile command</h2>

```
g++ src/solar.cpp src/glad.c \
   src/imgui.cpp src/imgui_draw.cpp src/imgui_widgets.cpp src/imgui_tables.cpp \
   src/imgui_impl_glfw.cpp src/imgui_impl_opengl3.cpp \
   -o Solar.exe \
   -Iinclude -Isrc \
   -Llib -lglfw3 -lgdi32 -lopengl32
```
<h2>run command:</h2>
  
```
./Solar.exe
```
