# CLAUDE.md - Build Instructions

## Coding Principles

- When writing code, prefer idiomatic C++17 and modern solutions.

### Testing Principles

- Only write unittests for non-trivial functionality (trivial: setters/getters etc.), unless specifically asked to do otherwise.

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

## Testing

### cfg2 Tests

When running project binaries, run them from the project root, by using relative path to the binary.
To build and run the cfg2 configuration system tests:

```bash
# Build the tests
cmake --build build --target cfg2_tests -- -j10

# Run all cfg2 tests
./build/cfg2_tests

# Run specific test (example)
./build/cfg2_tests --gtest_filter="*DynamicSectionsWithSameNameAsTypeAreHandledCorrectly*"
```

