/*
 * curlent - wget-like torrent downloader using libtorrent
 *
 * Usage:
 *     curlent <magnet_link_or_torrent_file> [-o OUTPUT_DIR]
 *
 * Compile:
 *     g++ -std=c++17 -o curlent curlent.cpp -ltorrent-rasterbar -lpthread
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <iomanip>
#include <csignal>
#include <fstream>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>

namespace fs = std::filesystem;
namespace lt = libtorrent;

static volatile sig_atomic_t interrupted = 0;

void signal_handler(int) {
    interrupted = 1;
}

struct Options {
    std::string input;
    std::string output_dir = ".";
    std::string interface;
    float seed_ratio = 2.0f;
    bool quiet = false;
    bool no_seed = false;
};

int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
}

int get_size_unit(std::int64_t bytes) {
    const int max_unit = 4;
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024 && unit_index < max_unit) {
        size /= 1024;
        unit_index++;
    }
    return unit_index;
}

std::string format_size(std::int64_t bytes, int force_unit = -1) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    if (force_unit >= 0) {
        for (int i = 0; i < force_unit && i < 4; i++) {
            size /= 1024;
        }
        unit_index = force_unit;
    } else {
        while (size >= 1024 && unit_index < 4) {
            size /= 1024;
            unit_index++;
        }
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

std::string format_size_padded(std::int64_t bytes, int width = 10) {
    std::string s = format_size(bytes);
    if (static_cast<int>(s.length()) < width) {
        s = std::string(width - s.length(), ' ') + s;
    }
    return s;
}

std::string make_progress_bar(float progress, int width) {
    int filled = static_cast<int>(progress * width);
    int empty = width - filled;

    std::string bar = "[";
    bar += "\033[32m"; // Green for filled
    bar += std::string(filled, '#');
    if (filled < width) {
        bar += '|';
        bar += "\033[90m"; // Gray for empty
        bar += std::string(empty > 0 ? empty - 1 : 0, ' ');
    }
    bar += "\033[0m"; // Reset
    bar += "]";
    return bar;
}

std::string format_time(int seconds) {
    if (seconds < 0 || seconds > 86400 * 365) {
        return "∞";
    }

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m " << secs << "s";
    } else if (minutes > 0) {
        oss << minutes << "m " << secs << "s";
    } else {
        oss << secs << "s";
    }
    return oss.str();
}

bool is_magnet(const std::string& input) {
    return input.find("magnet:") == 0;
}

std::string get_state_path() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    fs::path cache_dir = fs::path(home) / ".cache" / "curlent";
    fs::create_directories(cache_dir);
    return (cache_dir / "session_state").string();
}

void save_session_state(lt::session& ses, const std::string& path) {
    auto state = lt::write_session_params_buf(ses.session_state());
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(state.data(), static_cast<std::streamsize>(state.size()));
}

bool interface_up(const std::string& iface) {
    std::string path = "/sys/class/net/" + iface + "/operstate";
    std::ifstream ifs(path);
    if (!ifs) return false;

    std::string state;
    ifs >> state;
    return state == "up" || state == "unknown";
}

std::string get_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return (fs::path(home) / ".config" / "curlent" / "config").string();
}

std::string expand_tilde(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

void load_config(Options& opts) {
    std::string config_path = get_config_path();
    std::ifstream ifs(config_path);
    if (!ifs) return;

    std::string line;
    while (std::getline(ifs, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Remove leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t");
        size_t end = line.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // Parse key=value
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Remove whitespace around key and value
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        if (key == "output") {
            opts.output_dir = expand_tilde(value);
        } else if (key == "interface") {
            opts.interface = value;
        } else if (key == "ratio") {
            opts.seed_ratio = std::stof(value);
        } else if (key == "no-seed") {
            opts.no_seed = (value == "true" || value == "1");
        } else if (key == "quiet") {
            opts.quiet = (value == "true" || value == "1");
        }
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <magnet_link_or_torrent_file> [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  -o, --output DIR    Output directory (default: current directory)\n"
              << "  -i, --interface IF  Bind to network interface with kill switch (e.g. tun0, wg0)\n"
              << "  -r, --ratio RATIO   Seed ratio target (default: 2.0)\n"
              << "  -n, --no-seed       Exit after download, don't seed\n"
              << "  -q, --quiet         Quiet mode - minimal output\n"
              << "  -h, --help          Show this help\n"
              << "\n"
              << "Config file: ~/.config/curlent/config\n";
}

Options parse_args(int argc, char* argv[]) {
    Options opts;

    // Load config file first, command line args override
    load_config(opts);

    if (argc < 2) {
        print_usage(argv[0]);
        std::exit(1);
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "-q" || arg == "--quiet") {
            opts.quiet = true;
        } else if (arg == "-n" || arg == "--no-seed") {
            opts.no_seed = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an argument\n";
                std::exit(1);
            }
            opts.output_dir = expand_tilde(argv[++i]);
        } else if (arg == "-i" || arg == "--interface") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -i requires an argument\n";
                std::exit(1);
            }
            opts.interface = argv[++i];
        } else if (arg == "-r" || arg == "--ratio") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -r requires an argument\n";
                std::exit(1);
            }
            opts.seed_ratio = std::stof(argv[++i]);
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            std::exit(1);
        } else if (opts.input.empty()) {
            opts.input = arg;
        } else {
            std::cerr << "Error: unexpected argument: " << arg << "\n";
            std::exit(1);
        }
    }

    if (opts.input.empty()) {
        std::cerr << "Error: no input specified\n";
        print_usage(argv[0]);
        std::exit(1);
    }

    return opts;
}

const char* state_str(lt::torrent_status::state_t state) {
    switch (state) {
        case lt::torrent_status::checking_files: return "checking";
        case lt::torrent_status::downloading_metadata: return "metadata";
        case lt::torrent_status::downloading: return "downloading";
        case lt::torrent_status::finished: return "finished";
        case lt::torrent_status::seeding: return "seeding";
        case lt::torrent_status::checking_resume_data: return "checking";
        default: return "unknown";
    }
}

int download_torrent(const Options& opts) {
    // Session settings
    lt::settings_pack settings;
    settings.set_int(lt::settings_pack::alert_mask,
        lt::alert_category::error |
        lt::alert_category::status);

    // DHT bootstrap nodes
    settings.set_str(lt::settings_pack::dht_bootstrap_nodes,
        "router.bittorrent.com:6881,"
        "router.utorrent.com:6881,"
        "dht.transmissionbt.com:6881,"
        "dht.aelitis.com:6881");

    // Enable Local Service Discovery
    settings.set_bool(lt::settings_pack::enable_lsd, true);

    // Interface binding
    if (!opts.interface.empty()) {
        if (!interface_up(opts.interface)) {
            std::cerr << "Error: interface " << opts.interface << " is not up\n";
            return 1;
        }
        std::string listen = opts.interface + ":6881";
        settings.set_str(lt::settings_pack::listen_interfaces, listen);
        settings.set_str(lt::settings_pack::outgoing_interfaces, opts.interface);
    }

    // Try to load saved session state
    std::string state_path = get_state_path();
    lt::session_params sp;
    sp.settings = settings;

    if (fs::exists(state_path)) {
        try {
            std::ifstream ifs(state_path, std::ios::binary);
            std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), {});
            sp = lt::read_session_params(buf);
            sp.settings = settings;  // Override with our settings
        } catch (...) {
            // Use default params if load fails
        }
    }

    lt::session ses(sp);

    // Add extensions
    ses.add_extension(&lt::create_ut_metadata_plugin);
    ses.add_extension(&lt::create_ut_pex_plugin);
    ses.add_extension(&lt::create_smart_ban_plugin);

    // Create output directory
    fs::create_directories(opts.output_dir);

    // Add torrent
    lt::add_torrent_params params;
    params.save_path = opts.output_dir;

    if (is_magnet(opts.input)) {
        if (!opts.quiet) {
            std::cout << "Adding magnet link...\n";
        }
        params = lt::parse_magnet_uri(opts.input);
        params.save_path = opts.output_dir;
    } else {
        // It's a .torrent file
        if (!fs::exists(opts.input)) {
            std::cerr << "Error: file not found: " << opts.input << "\n";
            return 1;
        }

        if (!opts.quiet) {
            std::cout << "Loading torrent file: " << opts.input << "\n";
        }

        params.ti = std::make_shared<lt::torrent_info>(opts.input);
    }

    lt::torrent_handle handle = ses.add_torrent(params);

    // Wait for metadata
    while (!handle.status().has_metadata && !interrupted) {
        // Kill switch
        if (!opts.interface.empty() && !interface_up(opts.interface)) {
            save_session_state(ses, state_path);
            fprintf(stderr, "\n\nKill switch: interface %s is down\n", opts.interface.c_str());
            return 1;
        }

        if (!opts.quiet && is_magnet(opts.input)) {
            auto s = handle.status();
            auto ss = ses.session_state();
            fprintf(stderr, "\rWaiting for metadata... peers: %d, DHT nodes: %zu\033[K",
                s.num_peers, ss.dht_state.nodes.size() + ss.dht_state.nodes6.size());
            fflush(stderr);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!opts.quiet && is_magnet(opts.input)) {
        fprintf(stderr, "\n");
    }

    if (interrupted) {
        save_session_state(ses, state_path);
        std::cout << "\nInterrupted\n";
        return 130;
    }

    auto ti = handle.torrent_file();
    if (!opts.quiet && ti) {
        std::cout << "Name: " << ti->name() << "\n";
        std::cout << "Size: " << format_size(ti->total_size()) << "\n";
        std::cout << "Files: " << ti->num_files() << "\n\n";
    }

    // Download loop
    while (!interrupted) {
        // Kill switch
        if (!opts.interface.empty() && !interface_up(opts.interface)) {
            save_session_state(ses, state_path);
            fprintf(stderr, "\n\nKill switch: interface %s is down\n", opts.interface.c_str());
            return 1;
        }

        lt::torrent_status status = handle.status();

        if (status.is_seeding) {
            break;
        }

        if (!opts.quiet) {
            int term_width = get_terminal_width();

            std::int64_t downloaded = ti ? static_cast<std::int64_t>(ti->total_size() * status.progress) : 0;
            std::int64_t total = ti ? ti->total_size() : 0;

            // ETA
            std::string eta_str = "--:--";
            if (status.download_rate > 0 && total > 0) {
                int eta_secs = static_cast<int>((total - downloaded) / status.download_rate);
                eta_str = format_time(eta_secs);
            }

            // Format: "45% [====>    ] 12.3M/51.2M 1.2MB/s eta 2m30s"
            int percent = static_cast<int>(status.progress * 100);
            int size_unit = get_size_unit(total);
            std::string dl_str = format_size(downloaded, size_unit);
            std::string total_str = format_size(total, size_unit);
            std::string speed_str = format_size(status.download_rate) + "/s";

            // Calculate bar width based on fixed parts
            int fixed_len = 6 + 2 + dl_str.length() + 1 + total_str.length() + 1 + speed_str.length() + 5 + eta_str.length();
            int bar_width = term_width - fixed_len - 2;
            if (bar_width < 10) bar_width = 10;
            if (bar_width > 50) bar_width = 50;

            std::string bar = make_progress_bar(status.progress, bar_width);

            // Print to stderr with \r and clear to end of line
            fprintf(stderr, "\r%3d%% %s %s/%s %s eta %s\033[K",
                percent, bar.c_str(), dl_str.c_str(), total_str.c_str(),
                speed_str.c_str(), eta_str.c_str());
            fflush(stderr);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (interrupted) {
        save_session_state(ses, state_path);
        fprintf(stderr, "\n\nDownload interrupted\n");
        return 130;
    }

    if (!opts.quiet && ti) {
        fprintf(stderr, "\n\nDownload complete!\a\n");
        fprintf(stderr, "Saved to: %s\n", (fs::path(opts.output_dir) / ti->name()).string().c_str());
    }

    if (opts.no_seed) {
        save_session_state(ses, state_path);
        return 0;
    }

    if (!opts.quiet) {
        fprintf(stderr, "\nSeeding to ratio %.1f...\n\n", opts.seed_ratio);
    }

    // Seeding loop until target ratio
    std::int64_t total_size = ti ? ti->total_size() : 0;
    std::int64_t target_upload = static_cast<std::int64_t>(total_size * opts.seed_ratio);

    while (!interrupted) {
        // Kill switch
        if (!opts.interface.empty() && !interface_up(opts.interface)) {
            save_session_state(ses, state_path);
            fprintf(stderr, "\n\nKill switch: interface %s is down\n", opts.interface.c_str());
            return 1;
        }

        lt::torrent_status status = handle.status();
        std::int64_t uploaded = status.total_upload;
        float ratio = total_size > 0 ? static_cast<float>(uploaded) / total_size : 0;

        if (ratio >= opts.seed_ratio) {
            break;
        }

        if (!opts.quiet) {
            int term_width = get_terminal_width();

            int size_unit = get_size_unit(target_upload);
            std::string ul_str = format_size(uploaded, size_unit);
            std::string target_str = format_size(target_upload, size_unit);
            std::string speed_str = format_size(status.upload_rate) + "/s";

            // ETA
            std::string eta_str = "--:--";
            if (status.upload_rate > 0) {
                std::int64_t remaining = target_upload - uploaded;
                if (remaining > 0) {
                    int eta_secs = static_cast<int>(remaining / status.upload_rate);
                    eta_str = format_time(eta_secs);
                }
            }

            float progress = static_cast<float>(uploaded) / target_upload;
            if (progress > 1.0f) progress = 1.0f;
            int percent = static_cast<int>(progress * 100);

            // Calculate bar width
            int fixed_len = 6 + 2 + ul_str.length() + 1 + target_str.length() + 1 + speed_str.length() + 5 + eta_str.length();
            int bar_width = term_width - fixed_len - 2;
            if (bar_width < 10) bar_width = 10;
            if (bar_width > 50) bar_width = 50;

            std::string bar = make_progress_bar(progress, bar_width);

            fprintf(stderr, "\r%3d%% %s %s/%s %s eta %s\033[K",
                percent, bar.c_str(), ul_str.c_str(), target_str.c_str(),
                speed_str.c_str(), eta_str.c_str());
            fflush(stderr);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (interrupted) {
        save_session_state(ses, state_path);
        fprintf(stderr, "\n\nSeeding interrupted\n");
        return 130;
    }

    if (!opts.quiet) {
        fprintf(stderr, "\n\nSeeding complete!\a\n");
    }

    save_session_state(ses, state_path);
    return 0;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Options opts = parse_args(argc, argv);

    try {
        return download_torrent(opts);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
