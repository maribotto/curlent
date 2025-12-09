# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

curlent is a wget-like CLI torrent downloader using libtorrent-rasterbar. It downloads from magnet links or .torrent files, shows a colored progress bar, and seeds to ratio 2.0 after completion.

## Build Commands

```bash
make              # Build
make clean        # Clean build artifacts
make install      # Install to /usr/local/bin
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
./curlent -n file.torrent                 # Download only, no seeding
./curlent -q file.torrent                 # Quiet mode
```

Note: Magnet links must be quoted due to shell special characters (`?`, `&`).

## Architecture

Single-file C++ application (`curlent.cpp`):

- **Session setup**: Creates libtorrent session with DHT bootstrap nodes, LSD, and extensions (ut_metadata, ut_pex, smart_ban)
- **Session state**: Saved to `~/.cache/curlent/session_state` for faster DHT bootstrap on subsequent runs
- **Download loop**: Shows progress bar on stderr, updates every 500ms
- **Seeding loop**: After download completes, seeds until ratio 2.0 (skipped with `-n`)
- **Signal handling**: SIGINT/SIGTERM gracefully interrupt and save session state

Progress output uses ANSI escape codes for colors (green `#` for filled, gray for empty) and `\r\033[K` for single-line updates.
