# curlent

A wget-like command-line torrent downloader using libtorrent.

```
$ curlent archlinux-2025.12.01-x86_64.iso.torrent
Name: archlinux-2025.12.01-x86_64.iso
Size: 1.4 GB
Files: 1

 67% [#################################|                 ] 0.9 GB/1.4 GB 85.2 MB/s eta 6s
```

## Features

- Download from `.torrent` files or magnet links
- Colored progress bar with speed and ETA
- Seeds to ratio 2.0 after download (configurable)
- DHT, LSD, PEX for peer discovery
- Session state persistence for faster startup
- Terminal bell on completion

## Installation

### Dependencies

- libtorrent-rasterbar
- boost
- OpenSSL

**Arch Linux:**
```bash
sudo pacman -S libtorrent-rasterbar boost
```

**Debian/Ubuntu:**
```bash
sudo apt install libtorrent-rasterbar-dev libboost-all-dev libssl-dev
```

### Build

```bash
make
sudo make install
```

## Usage

```bash
# Download from .torrent file
curlent ubuntu.torrent

# Download from magnet link (must be quoted!)
curlent "magnet:?xt=urn:btih:..."

# Specify output directory
curlent -o ~/Downloads ubuntu.torrent

# Download only, don't seed
curlent -n ubuntu.torrent

# Quiet mode
curlent -q ubuntu.torrent
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Output directory (default: current) |
| `-n, --no-seed` | Exit after download, don't seed |
| `-q, --quiet` | Minimal output |
| `-h, --help` | Show help |

## License

MIT
