# Dynpax

C++ utility to turn your dynamic ELF binary into standalone packaged bundle

## Why it is useful?

Sometimes it is painful to create fully static build of C/C++ application, so one must collect all libraries by 
using output from `ldd` and manually or using scripts to copy all needed libraries into fake root directory.
This tool simplifies this process as well as add some additional features.

## Build/Compilation

### Dependencies

1. LIEF 0.17.6
2. CLI11 2.6.2
3. fmtlib 12.1.0

### Create docker image

1. Run `docker build -t smartcoder/dynpax .`

### Use docker image

See example of possible usage in `Dockerfile.example`

1. Run `docker build --progress=plain  -t example . -f Dockerfile.example`
2. Run `docker run --rm -it example bash`
