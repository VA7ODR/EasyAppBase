# EasyAppBase

Simplify building a Gui App by having all the heavy lifting of getting started done. ImGui is the basis of the Gui, running on SDL2 with OpenGL. It includes logging facilities, Thread management with tracking, mutex tracking and debug, core network facilities based on Boost, event handling, and more. 

Requires Boost, SDL, OpenGL and OpenSSL development libraries installed, and a compiler and libraries capable of C++23.

NOTE: Only tested in Ubuntu 23.10 Linux so far. 

## Use

Install Boost, SDL, OpenGL and OpenSSL development libraries, and a compiler and libraries capable of C++23.

Create an empty folder for your app:
```bash
mkdir YourApp
cd YourApp
git init
git submodule add https://github.com/VA7ODR/EasyAppBase
git submodule update --init --recursive
```

Next, copy 2 files from the EasyAppBase folder:
```bash
cp EasyAppBase/copy_this_as_your_=this_as_your_CMakeLists.txt
cp EasyAppBase/copy_this_as_your_main.cpp ./main.cpp
git add CMakeLists.txt
git add main.cpp
```

Edit the new CMakeLists.txt so the project name is yours. (YourApp for this example.)

Build:

```bash
mkdir build
cd build
cmake ../
make
./YourApp
```




