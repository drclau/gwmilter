# gwmilter

`gwmilter` is an email encryption gateway, designed to be deployed in front of an email server.
A typical deployment will consist of a combination of an MTA with milter support, and `gwmilter` running on the same or separate nodes.
It has been created circa 2014, and has been modernized and released as open-source in 2024.

It uses [Milter API](https://en.wikipedia.org/wiki/Milter) to integrate with MTAs, such as [Postfix](https://www.postfix.org) and [sendmail](https://en.wikipedia.org/wiki/Sendmail).
It supports multiple encryption protocols:
* [PGP](https://en.wikipedia.org/wiki/Pretty_Good_Privacy)
* [S/MIME](https://en.wikipedia.org/wiki/S/MIME)
* [PDF encryption](https://en.wikipedia.org/wiki/PDF/A)

PGP and S/MIME support relies on a local installation of [GnuPG](https://gnupg.org) for keys management.

> **NOTE:** `gwmilter` only handles the encryption of emails. For decrypting the emails, you need to have the private keys and an email client that supports PGP or S/MIME, depending on your case.
> <br>For PDFs, you will need to know the configured password and have a PDF viewer that supports encryption.<br><br>
> **IMPORTANT:**
> - for increased security, it is recommended that only the public part of the keys is stored for `gwmilter` to use.
> - losing your private keys will result in the incapacity of decrypting your emails. Be sure to back up your keys.

## Dependencies

* C++17 compiler
* cmake
* GnuPG
* [libegpgcrypt](https://github.com/rzvncj/libegpgcrypt)
* [libepdfcrypt](https://github.com/rzvncj/libepdfcrypt)
* boost (property_tree, regex, lexical_cast, algorithm/string)
* glib
* libmilter
* libcurl
* spdlog
* PkgConfig

## How to build

> **Notes on dependencies:** `libegpgcrypt` and `libepdfcrypt` may have to be built and installed from sources.
> <br>
> **macOS notes:** `libmilter` is available via [macports](https://www.macports.org) while the other dependencies are either part of the base system or can be installed via [brew](https://brew.sh) or [macports](https://www.macports.org).

After you resolve the dependencies and clone the repository locally, you can build the project as follows:

```shell
cmake -B build -S .
cmake --build build
```

If `libegpgcrypt` and `libepdfcrypt` are installed in a non-standard location, you can specify the path:

```shell
cmake -DEGPGCRYPT_PATH=../libs -DEPDFCRYPT_PATH=../libs -B build -S .
```

Should you want to build a debug binary, use:

```shell
cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .
```

## How to use

`gwmilter` uses a simple INI configuration file, comprised of a mandatory and reserved `[general]` section, and an unlimited number of encryption specification sections. The path to the INI file is specified with `-c` parameter (e.g. `./gwmilter -c config.ini`).

The `[general]` section contains options pertaining to how `gwmilter` interacts with the system and the MTA.

The encryption specification sections can have arbitrary names (e.g. `[pgp_example.com]`), 
and specify to which recipients they will apply (`match`), 
an encryption protocol (`encryption_protocol`) and protocol specific options.
A recipient is matched against each section's `match` setting.
The first section to match a recipient dictates how the email will be processed for that recipient.
If an email contains recipients that match multiple encryption sections 
(e.g. `rcpt_one@example.com` matches `[pgp_example.com]`, `rcpt_two@example.com` matches `[smime_example.com]`), 
the original email is encrypted using the settings from the first matching section and the email is altered before returning it to the MTA. 
However, a copy of the original email is made for each subsequent matching section, 
and each copy is processed according to the matching section's settings. 
These emails are then re-injected over SMTP to the MTA.
The recipients that do not match any section are dropped.

See the sample configuration for more details.

### Plans / TODO

* automated testing
* documentation (describe the architecture, add diagrams, describe the configuration options in detail etc.)
* MTA integration examples

# Authors

* Claudiu Dragalina-Paraipan ([drclau](https://github.com/drclau))