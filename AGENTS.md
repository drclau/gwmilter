# Repository Guidelines

## Project Structure & Module Organization
- Source: `src/` (key areas: `milter/`, `handlers/`, `smtp/`, `cfg/`, `cfg2/`, `utils/`, `logger/`). Entry point: `src/main.cpp`.
- Tests: `tests/` (pytest-based end-to-end tests). See `tests/README.md`.
- Integrations: `integrations/` (Docker Compose environments, dev helpers, keys).
- Build output: `build/` (created by CMake configure step).

## Build, Test, and Development Commands
- Configure + build (Release by default):
  - `cmake -DCMAKE_BUILD_TYPE=Release -DEGPGCRYPT_PATH=../libs -DEPDFCRYPT_PATH=../libs -B build -S .`
  - `cmake --build build -- -j10`
- Debug build:
  - `cmake -DCMAKE_BUILD_TYPE=Debug -DEGPGCRYPT_PATH=../libs -DEPDFCRYPT_PATH=../libs -B build -S .`
  - `cmake --build build -- -j10`
- Run locally: `./build/gwmilter -c config.ini` (or a generated `dev-config.ini`).
- Start standard end-to-end test environment: `docker compose -f integrations/docker-compose.yaml up -d`
- Run end-to-end tests locally: `pytest tests/` (use venv; see `tests/README.md`).
- Run end-to-end tests against containerized gwmilter: `pytest --gwmilter-container integrations-gwmilter-1 tests/`.

## Coding Style & Naming Conventions
- Formatter: `.clang-format` (LLVM-based; 4-space indent, column 120). Format with `clang-format -i <files>`.
- C++ standard: C++17. Headers use `.hpp`; implementation uses `.cpp`.
- Filenames: snake_case (e.g., `milter_message.cpp`). Prefer descriptive names; one class/major concept per file.

## Testing Guidelines
- Framework: pytest (integration/e2e). Python deps: `pip install -r tests/requirements.txt`.
- Environments: prefer Docker Compose setups in `integrations/`. For local runs, ensure `GNUPGHOME` and key imports per `DEV_GUIDE.md`.
- Conventions: place test data under `tests/eml/`; name tests `test_*.py`.

## Commit & Pull Request Guidelines
- Commits: imperative mood, scoped messages. Conventional prefixes welcome (e.g., `Fix:`, `Refactor:`). Keep changes focused.
- PRs: include a clear description, motivation, and testing notes; link issues; update docs (`README.md`, `DEV_GUIDE.md`, configs) and add/adjust tests when behavior changes.
- CI/local check: ensure project builds and `pytest` passes against the appropriate environment.

## Security & Configuration Tips
- Do not commit secrets or private keys. Use `integrations/keys/` generation flow for test keys.
- Prefer a dedicated `GNUPGHOME` for local runs. Point keyserver to `hkp://localhost:11371` when using the Dockerized key server.

