# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

curlent is a wget-like CLI torrent downloader using libtorrent-rasterbar. It downloads from magnet links or .torrent files, shows a colored progress bar, and seeds to a configurable ratio after completion.

## Build Commands

```bash
make              # Build
make clean        # Clean build artifacts
make install      # Install to /usr/local/bin + man page + config to ~/.config/curlent/
make uninstall    # Remove from /usr/local/bin
```

## Dependencies

- libtorrent-rasterbar (with development headers)
- boost (headers)
- OpenSSL
- C++17 compiler

On Arch Linux: `pacman -S libtorrent-rasterbar boost`

## Usage

```bash
./curlent file.torrent                    # Download from .torrent file
./curlent "magnet:?xt=urn:btih:..."       # Download from magnet (quote the URL!)
./curlent -o ~/Downloads file.torrent     # Specify output directory
./curlent -i tun0 file.torrent            # Bind to interface with kill switch
./curlent -r 1.0 file.torrent             # Custom seed ratio
./curlent -n file.torrent                 # Download only, no seeding
./curlent -q file.torrent                 # Quiet mode
```

Note: Magnet links must be quoted due to shell special characters (`?`, `&`).

## Configuration

Config file: `~/.config/curlent/config`

```ini
output = ~/Downloads
interface = wg0
ratio = 2.0
no-seed = false
quiet = false
```

Command line options override config file. Config is loaded in `load_config()`, parsed as `key = value` pairs.

## Architecture

Single-file C++ application (`curlent.cpp`):

- **Config loading**: Reads `~/.config/curlent/config` before parsing CLI args
- **Session setup**: Creates libtorrent session with DHT bootstrap nodes, LSD, and extensions (ut_metadata, ut_pex, smart_ban)
- **Interface binding**: Optional `-i` flag binds to specific interface using `listen_interfaces` and `outgoing_interfaces`
- **Kill switch**: Monitors `/sys/class/net/<iface>/operstate` every 500ms, exits if interface goes down
- **Session state**: Saved to `~/.cache/curlent/session_state` for faster DHT bootstrap on subsequent runs
- **Download loop**: Shows progress bar on stderr, updates every 500ms
- **Seeding loop**: After download completes, seeds until target ratio (default 2.0, configurable with `-r`)
- **Signal handling**: SIGINT/SIGTERM gracefully interrupt and save session state

Progress output uses ANSI escape codes for colors (green `#` for filled, gray for empty) and `\r\033[K` for single-line updates.
