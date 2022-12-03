# png2br
2-in-1 PNG to ASCII art converter and ASCII video player

## Requirements

* A C++20 capable compiler
* libavcodec
* libavutil
* libswscale
* libavformat
* libpng
* CMake

## Building

```sh
mkdir build && cd build
cmake ..
make
```

## Running

There are two projects, png2br and avtest. 

### png2br

A PNG to Unicode braille dot converter 

```sh
cd build
./png2br filename
```

### avtest

An ASCII video player

```sh
cd build
./avtest filename
```