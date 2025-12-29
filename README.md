# OpenDIBR V2

This repository contains a framework based on [OpenDIBR](https://github.com/IDLabMedia/open-dibr). Instead of decoding depth map videos during the rendering, simplified meshes are loaded in a the beginning of the application. One simplified mesh reconstructs the geometry for one video frame. The result is a depth-image-based renderer that achieves much higher frame rates.

Download the example dataset [here](https://cloud.ilabt.imec.be/index.php/s/iC4FoMrC9KGW8Ek) and put the files in the `dataset` folder. It contains 4*2 videos from the Interdigital Painter dataset.

### Dependencies

Since we use a variant of [OpenDIBR](https://github.com/IDLabMedia/open-dibr), most of the dependencies are the same:

* C++11 capable compiler (e.g. gcc/g++ or Visual Studio), CMake
* An NVIDIA GPU that supports hardware accelerated decoding for H265 4:2:0 (preferably up to 12-bit), [check your gpu here](https://en.wikipedia.org/wiki/NVDEC#GPU_support).
* CUDA Toolkit 10.2 or higher
* NVIDIA Video Codec SDK
* If you want to use Virtual Reality: Steam and SteamVR

For Linux systems, there are several libraries that need to be installed: FFmpeg, OpenGL, GLEW and SDL2. For example on Ubuntu:

```bash
sudo apt update
sudo apt install -y pkg-config libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libglew-dev libsdl2-2.0
```

### Preprocessing

To convert the depth map videos to meshes (one mesh per video frame), use the code in the `preprocessing` folder:

```bash
cd preprocessing
cmake . -B bin
cmake --build bin --config Release
```

Now run the executable, for example:

```bash
path\to\CreateMeshes.exe -i "../dataset/" -j "../dataset/config.json" -o "../dataset/meshes.bin" --triangle_deletion_margin 200 --gui -v
```

The `--gui` is optional, and opens a GUI showing the masks and edge maps. You should normally not include `--gui`, otherwhise the mesh creating will pause for every video frame. The `-v` or `--verbose` is also optional.

Note: **creating the meshes is quite slow**. For Painter, there are 300 video frames, so 300 meshes will be stored in `meshes.bin`. If you want, you can stop the preprocessing after 1 video frame and use `--static` during rendering (see below).

### Rendering

For the depth-image-based rendering code, go to the `open-dibr` folder:

```bash
cd open-dibr
cmake . -B bin
cmake --build bin --config Release
```

To run the renderer in `--static` mode (so only use the first frame of every video, and the first mesh in `meshes.bin`):

```bash
path\to\RealtimeDIBR.exe -i "../dataset/" -j "../dataset/config.json" -m "../dataset/meshes.bin" --static
```

For dynamic mode (so play the videos at 30 fps), omit `--static`:

```bash
path\to\RealtimeDIBR.exe -i "../dataset/" -j "../dataset/config.json" -m "../dataset/meshes.bin"
```

This assumes of course that `meshes.bin` contains the meshes for all video frames (300 for Painter).

**Controls:** While the application is running:

* press Esc to close the program
* use WASD and/or the mouse to move around
* use C and V to slow down or speed up the camera (or use --cam_speed)
* press M to toggle **wireframe** mode.

Code by [IDLab Media](https://media.idlab.ugent.be/).
