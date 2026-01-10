# Local Development and Debugging for **gwmilter**

This guide outlines the steps to set up a local development environment for **gwmilter**, allowing you to build, run, and debug it directly on your machine while using Dockerized services for dependencies like Postfix, Mailpit, and the key server.

For instructions on simply *running* the test suite against pre-defined Docker environments (including one where **gwmilter** itself is containerized), please refer to [`tests/README.md`](tests/README.md).

## 1. Build gwmilter

Before you can run **gwmilter** locally, you need to build it from source.

### a. Dependencies

Ensure the following dependencies are installed on your system:

*   C++17 compiler
*   cmake
*   GnuPG
*   [libegpgcrypt](https://github.com/rzvncj/libegpgcrypt)
*   [libepdfcrypt](https://github.com/rzvncj/libepdfcrypt)
*   boost (property_tree, lexical_cast, algorithm/string)
*   glib
*   libmilter
*   libcurl
*   spdlog
*   PkgConfig

> **Notes on dependencies:**
> *   `libegpgcrypt` and `libepdfcrypt` may have to be built and installed from their sources.
> *   **macOS notes:** `libmilter` is available via [macports](https://www.macports.org). Other dependencies are either part of the base system or can be installed via [brew](https://brew.sh) or [macports](https://www.macports.org).

### b. Building the Executable

After resolving the dependencies and cloning the repository:

1.  **Standard Build:**

    Navigate to the project root and run:
    ```shell
    cmake -B build -S .
    cmake --build build
    ```

2.  **Build with Custom Library Paths:**

    If `libegpgcrypt` and `libepdfcrypt` are installed in non-standard locations, specify their paths during CMake configuration:
    ```shell
    cmake -DEGPGCRYPT_PATH=/path/to/libegpgcrypt -DEPDFCRYPT_PATH=/path/to/libepdfcrypt -B build -S .
    cmake --build build
    ```
    Replace `/path/to/libegpgcrypt` and `/path/to/libepdfcrypt` with the actual paths to the installation directories of these libraries (e.g., where their `lib/` and `include/` or `share/cmake/` directories are found).

3.  **Debug Build:**

    To build a debug binary:
    ```shell
    cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .
    cmake --build build
    ```

> **Note:** Warnings are treated as errors by default. To disable this for local builds, configure with `-DWERROR=OFF`.

> **TIP:**<br>For faster builds specify the number of parallel jobs:<br>`cmake --build build -- -j4`<br>or use all available CPU cores:<br>`cmake --build build -- -j$(nproc)`

The **gwmilter** executable will be created in the `build/` directory (e.g., `./build/gwmilter`). Ensure you have a recent build of this executable, especially if you've made code changes.

## 2. Start Dependent Services

Use the `integrations/docker-compose-no-gwmilter.yaml` configuration. This file starts all necessary backend services (Postfix, Mailpit, Key Server, etc.) *without* starting **gwmilter** itself, thus allowing you to run your own local instance.

Navigate to the project root directory and run:
```sh
docker compose -f integrations/docker-compose-no-gwmilter.yaml up --build -d
```
The `-d` flag runs the containers in detached mode. You can monitor their logs if needed (e.g., `docker compose -f integrations/docker-compose-no-gwmilter.yaml logs -f key-server`).

**Note** that this will also start the `key-generator` service, which will populate `integrations/keys` directory with the keys configured in `key_list.txt`, and the signing-only key needed by **gwmilter** to sign re-injected emails in certain scenarios.

## 3. Prepare the Local **gwmilter** Runtime Environment

To run **gwmilter** locally in a way that's consistent with the test environment, you need to:
- Generate a suitable configuration file.
- Set up a dedicated GnuPG environment.

### a. Generate Development Configuration File

The `integrations/dev.sh` script generates a `dev-config.ini` file in the project root. This configures **gwmilter** to work with the dockerized Postfix.

Run from the project root:
```sh
./integrations/dev.sh
```

### b. Set Up Dedicated GnuPG Environment for **gwmilter**

It is recommended to isolate the GnuPG environment used by your local **gwmilter** instance. This prevents interference with your system's GnuPG keychain and ensures **gwmilter** uses test-specific keys and settings.

1.  **Choose and Create `GNUPGHOME` Directory:**

    For simplicity, use a directory within the `integrations` directory. It will serve as the `GNUPGHOME` for your local **gwmilter**.
    ```sh
    mkdir -p integrations/gnupg
    chmod 700 integrations/gnupg
    ```
    Ensure this directory is writable by the user who will run the **gwmilter** process. Any GnuPG files (keyrings, trustdb, `dirmngr.conf`) used by the local **gwmilter** will be stored here.

2.  **[optional] Configure `dirmngr.conf`:**

    Some tests expect GnuPG to retrieve a required missing public key from a remote keyserver. If you plan to run the tests, or to manually test the key retrieval feature, you need to configure GnuPG's `dirmngr` to use the dockerized keyserver as a source for public keys. To do so, make sure the following is the first keyserver setting in your `dirmngr.conf`:

    ```
    keyserver hkp://localhost:11371
    ```

    `dirmngr.conf` is located in your `GNUPGHOME` directory, or `~/.gnupg` if `GNUPGHOME` environment variable is not set.

3.  **Import **gwmilter**'s Internal Signing Key:**

    In certain scenarios, **gwmilter** requires a private key to sign emails before re-injecting them into the MTA. This key is generated by the `key-generator` service (its one-time run is a prerequisite) and placed in `integrations/keys/private/`. Import it into your dedicated `GNUPGHOME`:
    ```sh
    gpg --homedir integrations/gnupg --batch --import integrations/keys/private/gwmilter-signing-key.pgp
    ```
    *(The default `SIGNING_KEY_NAME` is `gwmilter-signing-key`)*.

4.  **Import Public Keys for Testing Scenarios:**
    The test suite expects certain public PGP keys to be present in **gwmilter**'s keyring at startup. These keys are also generated by the `key-generator` service and are located in `integrations/keys/public/`. The `entrypoint.sh` script in the Dockerized **gwmilter** imports keys matching the pattern `*present*.pgp`. Replicate this for your local setup:
    ```sh
    find integrations/keys/public -type f -name "*present*.pgp" -exec gpg --homedir integrations/gnupg --import {} \;
    ```
    Alternatively, you can import them individually by checking `integrations/key_list.txt` for keys intended to be "present" and finding their corresponding `.pgp` files in `integrations/keys/public/`. For example:
    ```sh
    gpg --homedir integrations/gnupg --import integrations/keys/public/pgp-valid-present-01@example.com.pgp
    ```

## 4. Run **gwmilter** Locally

With the configuration file generated and the GnuPG environment prepared, you can now run your locally built **gwmilter** executable. Set the `GNUPGHOME` environment variable for the **gwmilter** process and point it to the `dev-config.ini` file.

From the project root:
```sh
GNUPGHOME=integrations/gnupg ./build/gwmilter -c dev-config.ini
```
You should see log output from **gwmilter** in your terminal. You can also run this command under a debugger (e.g., `gdb --args`, or an IDE's debugger).

## Troubleshooting Local Development

*   **Permissions:** Double-check that the directory set as `GNUPGHOME` (e.g., `integrations/gnupg`) and its contents are writable by the user running the **gwmilter** process.
*   **GnuPG Key Issues:** If **gwmilter** complains about missing keys, verify that the import steps in "Set Up Dedicated GnuPG Environment" were successful for that specific `GNUPGHOME`. Use `gpg --homedir integrations/gnupg --list-keys` and `--list-secret-keys` to inspect.
*   **`dirmngr.conf`:** If **gwmilter** fails to fetch keys from the key server, ensure `integrations/gnupg/dirmngr.conf` exists and correctly points to `hkp://localhost:11371`. Also, check that the `key-server` container is running and accessible.
*   **`dev-config.ini`:** Open `dev-config.ini` and verify that `milter_socket` is correctly defined (e.g., `inet:10025@0.0.0.0`) and that `smtp_server` points to the Dockerized Postfix (e.g., `smtp://localhost:25`, though for local **gwmilter** this setting is for *outgoing* mail from **gwmilter** itself, not how Postfix connects *to* **gwmilter**).
*   **Postfix Configuration:** The Postfix service in `docker-compose-no-gwmilter.yaml` is configured with `SMTPD_MILTERS=inet:host.docker.internal:10025`. Ensure your local **gwmilter** is listening on `0.0.0.0:10025` or that `host.docker.internal` correctly resolves for your Docker setup.

This detailed guide should provide a solid foundation for local development and debugging of **gwmilter**. 
