# CLAUDE.md - Build Instructions

## Building the Project

To build the gwmilter project, use the following commands:

### Configure the Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DEGPGCRYPT_PATH=../libs -DEPDFCRYPT_PATH=../libs -B build -S . --fresh
```

### Build the Project

```bash
cmake --build build -- -j10
```

## Build Guidelines

- Run build commands and built binaries from project root by using relative paths

## Configuration Options

- `CMAKE_BUILD_TYPE=Debug` - Build in debug mode
- `EGPGCRYPT_PATH=../libs` - Path to the egpgcrypt library
- `EPDFCRYPT_PATH=../libs` - Path to the epdfcrypt library
- `-j10` - Use 10 parallel build jobs

## Testing Principles

- Only write unittests for non-trivial functionality (trivial: setters/getters etc.)
