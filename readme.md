# DigiAsset Core for Windows

> **This is a Windows port of [DigiAsset Core](https://github.com/DigiAsset-Core/DigiAsset_Core) originally created by [mctrivia](https://github.com/mctrivia).** All core logic, chain analysis, RPC methods, and DigiAsset protocol implementation are their work. This repository only adds Windows (MSVC) build support, platform-specific stubs, and a console dashboard UI.

## Table of Contents
1. [Build on Windows](#build-on-windows)
2. [Optional Build Targets](#optional-build-targets)
3. [Install DigiByte](#install-digibyte)
4. [Documentation](#Documentation)
5. [Other Notes](#other-notes)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Build on Windows

This fork builds a Windows version with Visual Studio and MSVC in the main branch, with upstream tracking in the 'upstream-master' branch. Upstream changes from [DigiAsset-Core/DigiAsset_Core](https://github.com/DigiAsset-Core/DigiAsset_Core) are merged periodically.

Most dependencies (libcurl, OpenSSL, SQLite3, libjsonrpccpp) are replaced by vendored source files or Windows-native stubs (WinHTTP), so no vcpkg or external package manager is needed beyond the jsoncpp and libjson-rpc-cpp subprojects that are already in the repo.

Note: If you want to skip building from source, download the pre-built binaries from the [Releases](https://github.com/chopperbriano/DigiAsset_Core_Windows/releases) page. You will still need to install IPFS Desktop and DigiByte Core wallet as described below. Run `DigiAssetCore.exe` from a cmd prompt — the web server is built in (no separate exe needed).

### Prerequisites

- **Visual Studio 2022** (Community or higher) with the "Desktop development with C++" workload
- **CMake 3.20+** (included with VS — select "C++ CMake tools for Windows" in the installer)

### Clone the Repository

```cmd
git clone --recursive https://github.com/chopperbriano/DigiAsset_Core_Windows.git
cd DigiAsset_Core_Windows
```

The `--recursive` flag is required to fetch the jsoncpp and libjson-rpc-cpp submodules. If you already cloned without it, run:

```cmd
git submodule update --init --recursive
```

### Build JsonCpp Library

```cmd
.\config-jsoncpp.bat
```

Open `jsoncpp\build\jsoncpp.sln` in Visual Studio. Select your build configuration (Debug or Release). Build `ALL_BUILD`, then build `INSTALL`.

### Build LibJson-RPC Library

```cmd
.\config-libjson-rpc.bat
```

Open `libjson-rpc-cpp\build\libjson-rpc-cpp.sln`. Use the **same** configuration as above. Build `ALL_BUILD`, then `INSTALL`.

### Install Boost (required for web server)

```cmd
nuget.exe install boost -Version 1.82.0 -OutputDirectory packages
```

If you don't have `nuget.exe`, download it from https://www.nuget.org/downloads

### Build DigiAsset Core

```cmd
.\config.bat
```

Open `build\digiasset_core.sln` in Visual Studio, select the **same** configuration (Debug or Release) as the libraries above, and build `ALL_BUILD`.

Or build from a Developer Command Prompt:

```cmd
cd build
msbuild src\DigiAssetCore.vcxproj /p:Configuration=Release
```

The `DigiAssetCore.exe` binary will be in `build\src\Release\` (or `Debug\`). This single executable includes the core sync engine, RPC server, and web UI server.

## Optional Build Targets

You can enable the CLI, Web server, and test suite by passing CMake options:

```cmd
cmake .. -DBUILD_CLI=ON -DBUILD_WEB=ON -DBUILD_TEST=ON
```

| Target | Binary | Description |
|---|---|---|
| `BUILD_CLI` | `DigiAssetCore-cli.exe` | Command-line RPC client |
| `BUILD_WEB` | `digiasset_core-web.exe` | Standalone web server (legacy, now built into main exe) |
| `BUILD_TEST` | `Google_Tests_run.exe` | Google Test suite |

### Performance Tuning

For faster initial blockchain sync, add to `config.cfg`:

```
verifydatabasewrite=0
```

This disables SQLite write verification (fsync), significantly reducing sync time.


## Install DigiByte

Download and install the latest verison of the DigiByte Core Wallet. https://github.com/DigiByte-Core/digibyte/releases/download/v8.22.2/digibyte-8.22.2-win64-setup.exe
Install to the default locations, unless you need to change the location on your hard drive. Then add the following lines to the digibyte.conf file.

```
rpcuser=user
rpcpassword=pass11
rpcbind=127.0.0.1
rpcport=14022
whitelist=127.0.0.1
rpcallowip=127.0.0.1
listen=1
server=1
txindex=1
deprecatedrpc=addresses
addnode=191.81.59.115
addnode=175.45.182.173
addnode=45.76.235.153
addnode=24.74.186.115
addnode=24.101.88.154
addnode=8.214.25.169
addnode=47.75.38.245
```


## Install IPFS

Download and install IPFS Desktop from https://github.com/ipfs/ipfs-desktop/releases

After installation, verify the IPFS API is running. The line "RPC API server listening on" shows the port (usually 5001). You can access the IPFS web UI at http://localhost:5001/webui in your browser.

## Configure DigiAsset Core

The first time you run DigiAsset Core for Windows it will ask you several questions to set up your config file. Run `DigiAssetCore.exe` from a cmd prompt:

```cmd
DigiAssetCore.exe
```

The single executable runs both the sync engine and the web UI server. The console displays a live dashboard with sync progress, service status, asset count, and a link to the web UI (default: http://localhost:8090/).

This will create config.cfg — the wizard creates only the basic config. For a full list of config options see example.cfg.

Make sure DigiAsset Core is running correctly and then press ctrl+c to stop it and continue with instructions.

NOTE: You will also need to open up two firewall ports:

```bash
Inbound TCP:5001
Inbound TCP:12024
```

## Documentation

The web UI is built into `DigiAssetCore.exe`. Once running, open http://localhost:8090/ in your browser.

## Credits

This project is a Windows port of **[DigiAsset Core](https://github.com/DigiAsset-Core/DigiAsset_Core)** by **[mctrivia](https://github.com/mctrivia)** and contributors. The core DigiAsset protocol implementation, chain analyzer, RPC interface, database schema, and all blockchain logic are entirely their work.

This fork adds only:
- Windows/MSVC build system and platform stubs (WinHTTP, OpenSSL stubs, vendored SQLite3)
- Console dashboard UI (VT100-based TUI)
- Embedded web server (no separate exe)
- Sync performance optimizations (prefetch pipeline, UTXO caching)

## Other Notes

- If submitting pull requests please utilize the .clang-format file to keep things standardized.
- Upstream changes are tracked on the `upstream-master` branch and merged into `master` periodically.

---

