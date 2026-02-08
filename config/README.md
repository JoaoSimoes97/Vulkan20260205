# Config folder

Two-file config: **default** (immutable) + **user** (mutable).

- **default.json** — Single source of default values. Created once by the app if missing; never overwritten. Do not edit for normal use.
- **config.json** — User config. Created from default if missing; can be updated by the app. Edit this to change settings.

Load order: user config is merged over default (missing keys in user = value from default). If the driver does not support a requested option (e.g. present mode, format), the app fails with a clear message; adjust config and restart.

Run the app from the **project root** or from the **install directory** (e.g. `install/Debug`) so that `config/config.json` and `config/default.json` are found.
