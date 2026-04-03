# pkgj

[![Total Downloads][img_downloads]][pkgj_downloads] [![Latest Release Downloads][img_latest_downloads]][pkgj_latest] [![Release][img_latest]][pkgj_latest] [![License][img_license]][pkgj_license]

This homebrew allows to download & install pkg files directly on PS Vita together with your [NoNpDrm][] or [NoPsmDrm][] fake license.
PSP games can be played using [Adrenaline][] or directly from LiveArea using [NoPspEmuDrm][].

# Features

* **works on** all PS Vita models, including PSTV.
* **easy** way to browse available downloads, with searching, filtering & sorting.
* **standalone** — no PC required, everything happens directly on Vita.
* **automatic** download and install — just choose an item and it will be installed, including a bubble in LiveArea.
* **background downloads** using the native bgdl function, so you can use the console while content downloads.
* **queues** multiple downloads.
* **supports** the TSV file format.
* **installs** PS Vita games, Game Updates, DLCs, Demos, Themes, PSM, PSP games (ISO or EBOOT), PSP DLCs, and PSX games.
* **game details view** with cover art, metadata, and personal notes for PS Vita and PSP games.
* **log viewer** with colour-coded levels (info / warning / error) and timestamps.
* **Linux simulator** for development (SDL2 + ImGui), with full keyboard mapping of Vita buttons.

# Download

Get the latest version as a [VPK file here][pkgj_latest].

# Usage

Make sure unsafe mode is enabled in HENkaku settings.

Select an item and press **X** to install, then follow the on-screen instructions. Press **Triangle** to open the sort / filter / search menu; press Triangle again to confirm or **O** to cancel.

Press **Left** / **Right** to move one page up or down.

# Configuration

pkgj ships with working default URLs. To customise settings, create `ux0:pkgj/config.txt` or `ur0:pkgj/config.txt`.

| Option | Description |
| --- | --- |
| `url_games <URL>` | PS Vita game list |
| `url_psv_demos <URL>` | PS Vita demo list |
| `url_dlcs <URL>` | PS Vita DLC list |
| `url_psv_themes <URL>` | PS Vita Theme list |
| `url_psm_games <URL>` | PS Mobile list |
| `url_psp_games <URL>` | PSP game list |
| `url_psp_dlcs <URL>` | PSP DLC list |
| `url_psx_games <URL>` | PSX game list |
| `url_comppack <URL>` | PS Vita compatibility pack list |
| `install_psp_psx_location uma0:` | Install PSP and PSX games on `uma0:` |
| `no_version_check 1` | Do not check for updates on startup |

> **Note:** `install_psp_as_pbp 1` is **obsolete**. The ISO / EBOOT choice is now available directly in the PSP game view.

# Q&A

1. **Where do I remove interrupted / failed downloads to free up space?**

    *PS Vita content:* Remove the queued download from LiveArea. If that doesn't work, delete the folder from `ux0:bgdl/t/` — each download is in a separate folder in queue order.

    *Everything else:* Open `ux0:pkgj/` — each download is in a separate folder named by its title ID. Delete the folder and resume file.

2. **Download speed is too slow!**

    Typical speeds are ~1–2 MB/s, which is normal for Vita hardware. Speed also depends on your WiFi router and signal strength. Packages containing many small files or folders may be slower because creating files/folders takes extra time.

3. **Can't background-download PSP games and they don't appear on LiveArea.**

    Background download and LiveArea bubbles for PSP games require the [NoPspEmuDrm][] plugin. Without it, the classic downloader is used and no bubble is created.

4. **I want to install PSP games as EBOOT files.**

    In the PSP game details view, choose **Install as EBOOT**. You will need the [npdrm_free][] plugin to play them.

5. **I can't play PSP games — error 80010087.**

    Install the [npdrm_free][] plugin in VSH, or install the game as ISO instead.

6. **PSM games don't work.**

    If you followed [NoPsmDrm][] instructions, try activating your account with [NoPsmDrm Fixer](https://github.com/Yoti/psv_npdrmfix).

7. **Can't download Updates or DLCs on PSTV.**

    This is caused by AntiBlackList. Completely uninstall it and install [DolcePolce](https://silica.codes/Li/dolcepolce) instead.

8. **How do I use compatibility packs?**

    Compatibility packs are disabled by default. It is recommended to use [reF00D](https://github.com/dots-tb/reF00D) or [0syscall6](https://github.com/SKGleba/0syscall6) instead. If you still want them, set `url_comppack` to `https://gitlab.com/nopaystation_repos/nps_compati_packs/raw/master/` in your config. Firmwares 3.65 or lower require a TLS workaround. Note: the list has not been updated since October 2019.

# Building

pkgj uses conan and cmake. The recommended way is to run `ci/ci.sh`, which creates a Python virtualenv with conan, sets up cross-compilation, registers recipes, and builds pkgj for Vita and `pkgj_cli` for testing.

**Prerequisites (Debian / equivalent):**

- build-essential, git-core, make, cmake
- python3-pip
- `pip3 install --user pipenv`
- ninja-build

pkgj is built in `ci/build/`. Rebuild any time by running `ninja` in that directory.

Set the `PSVITAIP` environment variable (before running cmake) to the IP address of your Vita to use `make send`, which copies `eboot.bin` directly to `ux0:app/PKGJ00000`.

To enable debug logging, pass `-DPKGI_ENABLE_LOGGING=ON` to cmake. The app will send log messages to UDP multicast `239.255.0.100:30000`. Receive them with [socat][] on your PC:

    $ socat udp4-recv:30000,ip-add-membership=239.255.0.100:0.0.0.0 -

**Linux simulator:**

Build the simulator with `./build.sh host` or `-DBUILD_SIM=ON` in CMake. It uses SDL2 + ImGui and maps keyboard keys to Vita buttons. All overlays (config editor, log viewer) work identically to the Vita build.

# Publishing a release (maintainers)

Push a tag in the form `v0.56` to create a release and build `pkgj.vpk`.

For a beta / pre-release, use a tag like `v0.56-beta1`. Pre-releases are not picked up by the auto-update.

# License

This software is released under the 2-clause BSD license.

`puff.h` and `puff.c` are under the [zlib][] license.

[NoNpDrm]: https://github.com/TheOfficialFloW/NoNpDrm/releases
[npdrm_free]: https://github.com/kyleatlast/npdrm_free/releases
[NoPsmDrm]: https://github.com/frangarcj/NoPsmDrm/
[NoPspEmuDrm]: https://github.com/LiEnby/NoPspEmuDrm
[Adrenaline]: https://github.com/TheOfficialFloW/Adrenaline
[socat]: http://www.dest-unreach.org/socat/
[zlib]: https://www.zlib.net/zlib_license.html
[pkgj_downloads]: https://github.com/toaster-code/pkgj/releases
[pkgj_latest]: https://github.com/toaster-code/pkgj/releases/latest
[pkgj_license]: https://github.com/toaster-code/pkgj/blob/master/LICENSE
[img_downloads]: https://img.shields.io/github/downloads/toaster-code/pkgj/total?label=Total%20Downloads&style=flat-square
[img_latest_downloads]: https://img.shields.io/github/downloads/toaster-code/pkgj/latest/total?label=Latest%20Release&style=flat-square
[img_latest]: https://img.shields.io/github/release/toaster-code/pkgj.svg?maxAge=3600
[img_license]: https://img.shields.io/github/license/toaster-code/pkgj.svg?maxAge=2592000
