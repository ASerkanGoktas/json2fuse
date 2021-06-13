# json2fuse

A fuse implementation which uses json file as a source of file structure. json2fuse works both ways. The changes you make to the json file will be visible in the file system, also, when you make changes in the file system, it will be written to json file.

## Dependencies
- `cJSON 1.7.14`
- `Fuse library`


## Build
Cmake finds the dependencies if you have the dependencies installed.

## Install
You can specify install directory by changing `CMAKE_INSTALL_PREFIX`

## Run

Run by `./json2fuse .. fuse parameters .. filestructure.json`


