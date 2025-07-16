# Test Runner Service

Lightweight Alpine-based container for running Python integration tests.

## Dependencies

- `python3` and `py3-pip`: For running pytest
- `gnupg`: For PGP operations in tests  
- `docker-cli`: For container interactions

## Usage

Used in `docker-compose-tests.yaml` to run the test suite:

```yaml
tests:
  build:
    dockerfile: integrations/tests-runner/Dockerfile
  volumes:
    - ../tests:/tests:rw
    - ../integrations/keys:/app/keys:ro
```

The `entrypoint.sh` script automatically sets up the Python environment and runs pytest. 