# Testing gwmilter

This document outlines the prerequisites, environment setup, and execution steps for running the **gwmilter** test suite.

For comprehensive instructions on setting up a local development environment to build, run, and debug **gwmilter** directly on your machine, please see [LOCAL_DEV.md](LOCAL_DEV.md).

## Prerequisites for Running Tests

1.  **Docker and Docker Compose:**
    
    Ensure Docker and Docker Compose are installed and running on your system. These are used to create an isolated test environment with all necessary services.
2.  **Python Environment (Recommended for running `pytest` locally):**
    
    If you plan to run `pytest` directly on your machine against the Dockerized services, it's highly recommended to use a Python virtual environment to manage dependencies.
    *   Create a virtual environment:
        ```sh
        python3 -m venv .venv
        source .venv/bin/activate
        ```
    *   Install Python dependencies:
        ```sh
        pip install -r tests/requirements.txt
        ```
3.  **GnuPG `dirmngr` Configuration (for local **gwmilter** testing):**
    
    If you plan to run **gwmilter** locally (outside of Docker) for testing against the dockerized services, you need to configure GnuPG's `dirmngr`. Ensure that your `dirmngr.conf` file (typically located in `~/.gnupg` or in `$GNUPGHOME` if it's set) has the dockerized `key-server` as the first entry:
    ```
    keyserver hkp://localhost:11371
    ```
    The `integrations/dev.sh` script can help check this configuration. For detailed GnuPG setup when running **gwmilter** locally for development, refer to [LOCAL_DEV.md](LOCAL_DEV.md).

## Environment Setup for Standard Test Execution

This section describes setting up the test environment where **gwmilter** itself also runs as a Docker container. This is the standard approach for most test execution scenarios.

*   Navigate to the project root directory.
*   Start the environment:
    ```sh
    docker compose -f integrations/docker-compose.yaml up --build -d
    ```
The `-d` flag runs the containers in detached mode. You can view logs using `docker compose -f integrations/docker-compose.yaml logs -f`.

If you are looking to run **gwmilter** locally for development and debugging, please refer to the detailed instructions in [LOCAL_DEV.md](LOCAL_DEV.md). That guide covers starting dependent services with `docker-compose-no-gwmilter.yaml` and all necessary local **gwmilter** setup.

## Running the Tests

This section assumes you are running `pytest` from your local machine.

**1. Test Environment:**

   Ensure the appropriate Docker Compose environment is running:
   *   For standard tests (gwmilter in Docker): `docker compose -f integrations/docker-compose.yaml up -d`
   *   If testing a locally running **gwmilter**: Ensure dependent services are up via `docker-compose-no-gwmilter.yaml` and your local **gwmilter** is running. See [LOCAL_DEV.md](LOCAL_DEV.md) for this setup.

**2. Executing `pytest` Locally:**

   Once the environment is running:
   *   **Ensure your current directory is the project root (**gwmilter**).**
   *   **If using a Python virtual environment, make sure it's activated.**
       ```sh
       source .venv/bin/activate
       ```
   *   **Run `pytest`:**

       The command depends on how **gwmilter** is running:

       *   **If **gwmilter** is running inside a Docker container** (via `integrations/docker-compose.yaml`):

           You **must** specify the name of the **gwmilter** container. This allows test utilities to correctly target the containerized **gwmilter** environment. The default container name is `integrations-gwmilter-1`.
           ```sh
           pytest --gwmilter-container integrations-gwmilter-1 tests/
           ```
           Alternatively, you can add this to your `tests/pytest.ini` file:
           ```ini
           [pytest]
           # ... other options ...
           gwmilter_container = integrations-gwmilter-1
           ```

       *   **If **gwmilter** is running locally:**

           To test against a **gwmilter** instance running directly on your machine (after following setup in [LOCAL_DEV.md](LOCAL_DEV.md)), ensure your `pytest.ini` does not specify a `gwmilter_container`, or use the `--gwmilter-local` flag:
           ```sh
           pytest --gwmilter-local tests/
           # Or, if pytest.ini has no gwmilter_container defined:
           pytest tests/
           ```
   Pytest will automatically discover and run the tests located in the `tests` directory, according to the configuration in `tests/pytest.ini`.

### Alternative: Fully Containerized Test Execution (for CI/Convenience)

As an alternative, especially for CI pipelines or if you prefer not to manage a local Python test environment, you can run the entire test suite within Docker using `integrations/docker-compose-tests.yaml`. This file extends the main Docker Compose setup to include a `tests` service that executes `pytest` internally.

*   **Ensure no other conflicting services** (e.g., from `docker-compose.yaml` or `docker-compose-no-gwmilter.yaml`) are running on the same ports.
*   **Navigate to the project root directory.**
*   **Execute the tests:**
    ```sh
    docker compose -f integrations/docker-compose-tests.yaml up tests --build
    ```
    This command will build images, start all services, run `pytest` in the `tests` container, and then stop. Test output is shown in the console. To re-run, it's best to `down` the environment first.

## Troubleshooting

*   **Inspecting Emails After Tests:**

    If tests are failing and you suspect issues with email content, encryption, or delivery, you can prevent automatic email cleanup in `Mailpit`. Run pytest with the `--disable-email-cleanup` flag:
    ```bash
    pytest --disable-email-cleanup tests/
    # or, if using a container
    pytest --gwmilter-container integrations-gwmilter-1 --disable-email-cleanup tests/
    ```
    After the tests complete (or fail), navigate to the Mailpit UI (`http://localhost:8025`) in your browser to inspect the emails that were sent during the test run. This can provide valuable clues about what might have gone wrong.

## Cleaning Up

*   To stop the Docker Compose services:
    *   If you used `docker-compose.yaml`:
        ```sh
        docker compose -f integrations/docker-compose.yaml down
        ```
    *   If you used `docker-compose-no-gwmilter.yaml`:
        ```sh
        docker compose -f integrations/docker-compose-no-gwmilter.yaml down
        ```
    *   If you used `docker-compose-tests.yaml`:
        ```sh
        docker compose -f integrations/docker-compose-tests.yaml down
        ```
*   To remove volumes (e.g., to clear GnuPG data or generated keys), add the `-v` flag to any of the `down` commands above, for example:
    ```sh
    docker compose -f integrations/docker-compose.yaml down -v
    # or
    docker compose -f integrations/docker-compose-tests.yaml down -v
    ```
*   To delete the generated keys entirely:
    ```sh
    rm -fr integrations/keys
    ```
*   If you ran **gwmilter** locally, stop the process (e.g., with `Ctrl+C`).
*   If you used a Python virtual environment, you can deactivate it:
    ```sh
    deactivate
    ```
