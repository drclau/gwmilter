# Testing gwmilter

This document outlines the prerequisites, environment setup, and execution steps for running the `gwmilter` test suite.

## Prerequisites

1.  **Docker and Docker Compose:** Ensure Docker and Docker Compose are installed and running on your system. These are used to create an isolated test environment with all necessary services.
2.  **Python Environment (Optional but Recommended):**
    While the tests can be run with a system Python, it's highly recommended to use a Python virtual environment to manage dependencies.
    *   Create a virtual environment:
        ```sh
        python3 -m venv .venv
        source .venv/bin/activate
        ```
    *   Install Python dependencies:
        ```sh
        pip install -r tests/requirements.txt
        ```
3.  **GnuPG `dirmngr` Configuration (for local `gwmilter` testing):**
    If you plan to run `gwmilter` locally (outside of Docker) for testing against the Dockerized services, you need to configure GnuPG's `dirmngr`. Ensure that your `dirmngr.conf` (typically located in `~/.gnupg` or in `$GNUPGHOME` if it's set) file has the local key server as the first entry:
    ```
    keyserver hkp://localhost:11371
    ```
    The `integrations/dev.sh` script can help check this configuration.

## Environment Setup for Tests

The test environment relies on Docker Compose to manage the required services (Postfix, Mailpit, Key Server, etc.). There are two primary ways to set up the environment:

1.  **Running `gwmilter` within Docker (Standard Approach):**
    This is the recommended approach for most testing scenarios. It runs all components, including `gwmilter` itself, as Docker containers.
    *   Navigate to the project root directory.
    *   Start the environment:
        ```sh
        docker compose -f integrations/docker-compose.yaml up --build -d
        ```
    The `-d` flag runs the containers in detached mode. You can view logs using `docker compose -f integrations/docker-compose.yaml logs -f`.

2.  **Running `gwmilter` on the Host (for Development/Debugging):**
    This setup is useful when you are actively developing or debugging `gwmilter` locally and want to test it against the other services running in Docker.
    *   Navigate to the project root directory.
    *   Start the dependent services:
        ```sh
        docker compose -f integrations/docker-compose-no-gwmilter.yaml up --build -d
        ```
    *   Generate the `gwmilter` configuration for local development (this will create `dev-config.ini` in the project root):
        ```sh
        ./integrations/dev.sh
        ```
    *   Build `gwmilter` as described [here](https://github.com/drclau/gwmilter/blob/main/README.md).
    *   Run `gwmilter` locally, pointing to the generated configuration:
        ```sh
        ./build/gwmilter -c dev-config.ini
        ```

## Running the Tests

Once the prerequisites are met and the appropriate test environment is running:

1.  **Ensure your current directory is the project root (`gwmilter`).**
2.  **If using a Python virtual environment, make sure it's activated.**
    ```sh
    source .venv/bin/activate
    ```
3.  **Run Pytest:**

    The command to run tests depends on how `gwmilter` is running:

    *   **If `gwmilter` is running inside a Docker container** (i.e., you used `integrations/docker-compose.yaml`):
        You **must** specify the name of the `gwmilter` container. This allows test utilities (like `pgp-utils.sh` used for key management) to correctly target the containerized `gwmilter` environment. The default container name is `integrations-gwmilter-1`.
        ```sh
        pytest --gwmilter-container integrations-gwmilter-1 tests/
        ```
        Alternatively, you can add this to your `tests/pytest.ini` file:
        ```ini
        [pytest]
        # ... other options ...
        gwmilter_container = integrations-gwmilter-1
        ```

    *   **If `gwmilter` is running directly on the host** (i.e., you used `integrations/docker-compose-no-gwmilter.yaml` and started `gwmilter` manually):
        If your `pytest.ini` file does *not* specify a `gwmilter_container`, the tests will default to interacting with a local `gwmilter` setup. In this scenario, you typically do not need any extra flags:
        ```sh
        pytest tests/
        ```
        However, if `pytest.ini` *does* specify a `gwmilter_container` and you want to test against a local `gwmilter` instance, you **must** use the `--gwmilter-local` flag to override the INI setting:
        ```sh
        pytest --gwmilter-local tests/
        ```

    Pytest will automatically discover and run the tests located in the `tests` directory, according to the configuration in `tests/pytest.ini`.

    The tests will interact with the Postfix service running on `localhost:25` and the Mailpit service on `http://localhost:8025` (as configured in `tests/pytest.ini`).

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
*   To remove volumes (e.g., to clear GnuPG data or generated keys):
    ```sh
    docker compose -f integrations/docker-compose.yaml down -v
    ```
*   If you ran `gwmilter` locally, stop the process (e.g., with `Ctrl+C`).
*   If you used a Python virtual environment, you can deactivate it:
    ```sh
    deactivate
    ```
