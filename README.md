# gwmilter

`gwmilter` is an email encryption gateway, designed to be deployed in front of an email server.
A typical deployment will consist of a combination of an MTA with milter support, and `gwmilter` running on the same or separate nodes.
It has been created circa 2014, and has been modernized and released as open-source in 2024.

It uses [Milter API](https://en.wikipedia.org/wiki/Milter) to integrate with MTAs, such as [Postfix](https://www.postfix.org) and [sendmail](https://en.wikipedia.org/wiki/Sendmail).
It supports multiple encryption protocols:
* [PGP](https://en.wikipedia.org/wiki/Pretty_Good_Privacy)
* [S/MIME](https://en.wikipedia.org/wiki/S/MIME)
* PDF encryption

PGP and S/MIME support relies on a local installation of [GnuPG](https://gnupg.org) for keys management.

> **NOTE:** `gwmilter` only handles the encryption of emails. For decrypting the emails, you need to have the private keys and an email client that supports PGP or S/MIME, depending on your case.
> <br>For PDFs, you will need to know the configured password and have a PDF viewer that supports encryption.<br><br>
> **IMPORTANT:**
> - for increased security, it is recommended that only the public part of the keys is stored for `gwmilter` to use.
> - losing your private keys will result in the incapacity of decrypting your emails. Be sure to back up your keys.

## Getting Started

- Build and run: see [`DEV_GUIDE.md`](DEV_GUIDE.md).
- Integration environment (Docker Compose): see [`integrations/README.md`](integrations/README.md).
- Automated end-to-end tests: see [`tests/README.md`](tests/README.md).

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
These emails are then re-injected over SMTP into the MTA.
The recipients that do not match any section are dropped.

See [`config.ini`](https://github.com/drclau/gwmilter/blob/main/config.ini) and [`integrations/gwmilter/config.ini.template`](https://github.com/drclau/gwmilter/blob/main/integrations/gwmilter/config.ini.template) for more details.

# Authors

* Claudiu Dragalina-Paraipan ([drclau](https://github.com/drclau))