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
- Seeds to target ratio after download (default 2.0)
- Interface binding with kill switch for VPN use
- Configuration file for persistent settings
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

# Bind to VPN interface with kill switch
curlent -i tun0 ubuntu.torrent

# Custom seed ratio
curlent -r 1.0 ubuntu.torrent

# Download only, don't seed
curlent -n ubuntu.torrent

# Quiet mode
curlent -q ubuntu.torrent
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output DIR` | Output directory (default: current) |
| `-i, --interface IF` | Bind to network interface with kill switch |
| `-r, --ratio RATIO` | Seed ratio target (default: 2.0) |
| `-n, --no-seed` | Exit after download, don't seed |
| `-q, --quiet` | Minimal output |
| `-h, --help` | Show help |

## Configuration

Settings can be persisted in `~/.config/curlent/config`:

```ini
# Output directory
output = ~/Downloads

# Bind to network interface with kill switch
interface = wg0

# Seed ratio target
ratio = 1.5

# Exit after download, don't seed
no-seed = false

# Quiet mode
quiet = false
```

Command line options override config file settings.

## Kill Switch

When using `-i`, curlent binds all traffic to the specified interface and monitors its status. If the interface goes down (e.g., VPN disconnects), curlent immediately stops to prevent IP leaks.

## Man Page

After installation, documentation is available via `man curlent`.

## License

[MIT](LICENSE)
