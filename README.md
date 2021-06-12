# json2fuse

A fuse implementation which uses json file as a source of file structure. The filesystem is read-only, however, it can be changed by changing the input file-structure json file.

## Dependencies
- `cJSON 1.7.14`
- `Fuse library`


## Build
Cmake finds the dependencies if you have the dependencies installed.

## Install
You can specify install directory by changing `CMAKE_INSTALL_PREFIX`

## Run

Run by `./json2fuse .. fuse parameters .. filestructure.json`


