#!/usr/bin/env bash
set -e

# Wargames Movie Simulator
# Cross-platform installer for Linux and macOS

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OS="$(uname -s)"
ARCH="$(uname -m)"

echo "Wargames Movie Simulator installer"
echo "Platform: ${OS} (${ARCH})"
echo

have() {
    command -v "$1" >/dev/null 2>&1
}

install_linux_dependencies() {
    echo "Installing Linux dependencies..."

    if have apt-get; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            git \
            libncurses5-dev \
            lynx \
            curl \
            wget \
            netcat-openbsd \
            socat \
            fonts-dejavu-core

        # Telnet is optional because telehack.sh can fall back to nc.
        if apt-cache show telnet >/dev/null 2>&1; then
            sudo apt-get install -y telnet
        fi

        # cool-retro-term is used by the CRT launcher.
        if apt-cache show cool-retro-term >/dev/null 2>&1; then
            sudo apt-get install -y cool-retro-term
        else
            echo "WARNING: cool-retro-term is not available from this distribution's apt repository."
            echo "The simulator will still work, but ./imsai8080-crt.sh will be unavailable."
        fi
    else
        echo "ERROR: This Linux installer currently supports Debian/Ubuntu-style systems using apt-get."
        exit 1
    fi
}

install_macos_dependencies() {
    echo "Checking macOS dependencies..."

    if ! xcode-select -p >/dev/null 2>&1; then
        echo "Xcode Command Line Tools are required."
        echo "Starting Apple's installer..."
        xcode-select --install || true
        echo
        echo "Re-run this installer after the Command Line Tools installation completes."
        exit 1
    fi

    if ! have brew; then
        echo "ERROR: Homebrew is required to install Lynx on macOS."
        echo "Install Homebrew, then run this installer again."
        exit 1
    fi

    brew update
    brew install lynx ncurses pkg-config socat cmake

    # macOS already provides nc, which telehack.sh can use if telnet is absent.
}

install_cool_retro_term() {
    echo "Checking cool-retro-term..."

    if [ "$OS" = "Darwin" ]; then
        local app="/Applications/cool-retro-term.app"

        if [ -d "$app" ]; then
            echo "cool-retro-term already installed: $app"
            return
        fi

        echo "Installing cool-retro-term from the upstream GitHub release..."

        local api_url="https://api.github.com/repos/Swordfish90/cool-retro-term/releases/latest"
        local dmg_url
        local tmpdir
        local dmg
        local mountpoint

        dmg_url="$(
            curl -fsSL "$api_url" |
            python3 -c '
import json, sys
data = json.load(sys.stdin)
for asset in data.get("assets", []):
    name = asset.get("name", "")
    url = asset.get("browser_download_url", "")
    if name.lower().endswith(".dmg") and url:
        print(url)
        break
'
        )"

        if [ -z "$dmg_url" ]; then
            echo "ERROR: Could not locate a macOS DMG in the latest cool-retro-term GitHub release." >&2
            exit 1
        fi

        tmpdir="$(mktemp -d)"
        dmg="$tmpdir/cool-retro-term.dmg"
        mountpoint="$tmpdir/mount"
        mkdir -p "$mountpoint"

        echo "Downloading: $dmg_url"
        curl -fL "$dmg_url" -o "$dmg"

        hdiutil attach "$dmg" -mountpoint "$mountpoint" -nobrowse -quiet

        local source_app
        source_app="$(find "$mountpoint" -maxdepth 2 -type d -name "cool-retro-term.app" -print -quit)"

        if [ -z "$source_app" ]; then
            hdiutil detach "$mountpoint" -quiet || true
            rm -rf "$tmpdir"
            echo "ERROR: cool-retro-term.app was not found inside the downloaded DMG." >&2
            exit 1
        fi

        echo "Copying cool-retro-term.app to /Applications..."
        rm -rf "$app"

        if cp -R "$source_app" /Applications/ 2>/dev/null; then
            :
        else
            echo "Administrator permission is required to install cool-retro-term in /Applications."
            sudo cp -R "$source_app" /Applications/
        fi

        hdiutil detach "$mountpoint" -quiet || true
        rm -rf "$tmpdir"

        if [ ! -x "$app/Contents/MacOS/cool-retro-term" ]; then
            echo "ERROR: cool-retro-term installation completed, but its executable was not found." >&2
            exit 1
        fi

        echo "cool-retro-term installed successfully."
        return
    fi

    if have cool-retro-term; then
        echo "cool-retro-term installed: $(command -v cool-retro-term)"
    else
        echo "WARNING: cool-retro-term is not installed."
    fi
}


download_file() {
    local url="$1"
    local dest="$2"

    if have curl; then
        curl -L "$url" -o "$dest"
    elif have wget; then
        wget "$url" -O "$dest"
    else
        echo "ERROR: curl or wget is required."
        exit 1
    fi
}

install_vidtex() {
    if have vidtex; then
        echo "Vidtex already installed: $(command -v vidtex)"
        return
    fi

    echo "Installing Vidtex..."

    local archive="vidtex-1.3.0.tar.gz"
    local srcdir="vidtex-1.3.0"
    local url="https://github.com/simonlaszcz/vidtex/blob/master/releases/vidtex-1.3.0.tar.gz?raw=true"

    if [ ! -f "$archive" ]; then
        download_file "$url" "$archive"
    fi

    rm -rf "$srcdir"
    tar xvf "$archive"

    if [ "$OS" = "Darwin" ]; then
        echo "Applying macOS Vidtex compatibility patch..."

        python3 - "$srcdir/src/main.c" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

def must_replace(old, new, label):
    global text
    if old not in text:
        raise SystemExit(f"ERROR: Vidtex patch target not found: {label}")
    text = text.replace(old, new, 1)

must_replace('#include <sys/timerfd.h>\n', '', 'timerfd include')

if '#include <stdint.h>\n' not in text:
    must_replace('#include <stdlib.h>\n', '#include <stdlib.h>\n#include <stdint.h>\n', 'stdint include')

if '#include <limits.h>\n' not in text:
    must_replace('#include <locale.h>\n', '#include <locale.h>\n#include <limits.h>\n', 'limits include')

must_replace('    int flash_timer_fd;\n', '', 'flash_timer_fd field')
must_replace('    session.flash_timer_fd = -1;\n', '', 'flash_timer_fd init')

decl = 'static void vt_save(struct vt_session_state *session);\n'
if 'static uint64_t vt_now_ms(void);' not in text:
    must_replace(
        decl,
        decl + 'static uint64_t vt_now_ms(void);\nstatic int vt_flash_poll_timeout(uint64_t next_flash_ms);\n',
        'helper declarations'
    )

must_replace(
'''    session.flash_timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if (session.flash_timer_fd == -1) {
        log_err();
        goto abend;
    }

    struct itimerspec flash_time = {{1, 0}, {1, 0}};
    if (timerfd_settime(session.flash_timer_fd, TFD_TIMER_ABSTIME, &flash_time, NULL) == -1) {
        log_err();
        goto abend;
    }

''',
'',
'main timer setup'
)

must_replace(
'''    struct pollfd poll_data[3] = {
        {.fd = session.socket_fd, .events = POLLIN},
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = session.flash_timer_fd, .events = POLLIN}
    };

    while (!(terminate_received || socket_closed)) {
        int prv = poll(poll_data, 3, POLL_PERIOD_MS);
''',
'''    struct pollfd poll_data[2] = {
        {.fd = session.socket_fd, .events = POLLIN},
        {.fd = STDIN_FILENO, .events = POLLIN}
    };

    uint64_t next_flash_ms = vt_now_ms() + 1000;

    while (!(terminate_received || socket_closed)) {
        int prv = poll(poll_data, 2, vt_flash_poll_timeout(next_flash_ms));
''',
'main poll block'
)

must_replace(
'''        if (poll_data[2].revents & POLLIN) {
            uint64_t elapsed = 0;
            if (read(session.flash_timer_fd, &elapsed, sizeof(uint64_t)) > 0) {
                vt_decoder_toggle_flash(&session.decoder_state);
            }
        }
''',
'''        uint64_t now_ms = vt_now_ms();
        if (now_ms >= next_flash_ms) {
            vt_decoder_toggle_flash(&session.decoder_state);
            do {
                next_flash_ms += 1000;
            } while (next_flash_ms <= now_ms);
        }
''',
'main flash handler'
)

text = text.replace(
'''    if (session.flash_timer_fd > -1) {
        if (close(session.flash_timer_fd) == -1) {
            log_err();
        }
    }

''',
''
)

must_replace(
'''    state->flash_timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if (state->flash_timer_fd == -1) {
        log_err();
        goto abend;
    }

    struct itimerspec flash_time = {{1, 0}, {1, 0}};
    if (timerfd_settime(state->flash_timer_fd, TFD_TIMER_ABSTIME, &flash_time, NULL) == -1) {
        log_err();
        goto abend;
    }

''',
'',
'show timer setup'
)

must_replace(
'''    struct pollfd poll_data[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = state->flash_timer_fd, .events = POLLIN}
    };

    while (!terminate_received) {
        int prv = poll(poll_data, 2, POLL_PERIOD_MS);
''',
'''    struct pollfd poll_data[1] = {
        {.fd = STDIN_FILENO, .events = POLLIN}
    };

    uint64_t next_flash_ms = vt_now_ms() + 1000;

    while (!terminate_received) {
        int prv = poll(poll_data, 1, vt_flash_poll_timeout(next_flash_ms));
''',
'show poll block'
)

must_replace(
'''        if (poll_data[1].revents & POLLIN) {
            uint64_t elapsed = 0;
            if (read(state->flash_timer_fd, &elapsed, sizeof(uint64_t)) > 0) {
                vt_decoder_toggle_flash(&state->decoder_state);
            }
        }
''',
'''        uint64_t now_ms = vt_now_ms();
        if (now_ms >= next_flash_ms) {
            vt_decoder_toggle_flash(&state->decoder_state);
            do {
                next_flash_ms += 1000;
            } while (next_flash_ms <= now_ms);
        }
''',
'show flash handler'
)

text = text.replace('#define POLL_PERIOD_MS      (-1)\n', '')

helpers = '''
static uint64_t
vt_now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static int
vt_flash_poll_timeout(uint64_t next_flash_ms)
{
    uint64_t now_ms = vt_now_ms();

    if (now_ms >= next_flash_ms) {
        return 0;
    }

    uint64_t remaining = next_flash_ms - now_ms;
    if (remaining > (uint64_t)INT_MAX) {
        return INT_MAX;
    }

    return (int)remaining;
}

'''

anchor = 'static void\nvt_usage(void)\n'
if 'static uint64_t\nvt_now_ms(void)' not in text:
    must_replace(anchor, helpers + anchor, 'helper insertion')

path.write_text(text, encoding="utf-8")
print(f"Patched {path}")
PY
    fi

    (
        cd "$srcdir"

        if [ "$OS" = "Darwin" ]; then
            NCURSES_PREFIX="$(brew --prefix ncurses)"

            export CPPFLAGS="-I${NCURSES_PREFIX}/include ${CPPFLAGS:-}"
            export CFLAGS="-I${NCURSES_PREFIX}/include ${CFLAGS:-}"
            export LDFLAGS="-L${NCURSES_PREFIX}/lib ${LDFLAGS:-}"
            export PKG_CONFIG_PATH="${NCURSES_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"

            ./configure
            make

            PREFIX="$(brew --prefix)"
            make install prefix="$PREFIX" || sudo make install
        else
            ./configure
            make
            sudo make install
        fi
    )
}

install_fonts() {
    echo "Installing optional retro terminal fonts..."

    local bedstead="bedstead.otf"
    local mode7="MODE7GX3.TTF"

    [ -f "$bedstead" ] || download_file "https://bjh21.me.uk/bedstead/bedstead.otf" "$bedstead"
    [ -f "$mode7" ] || download_file "https://galax.xyz/TELETEXT/MODE7GX3.TTF" "$mode7"

    if [ "$OS" = "Darwin" ]; then
        mkdir -p "$HOME/Library/Fonts"
        cp -f "$bedstead" "$HOME/Library/Fonts/"
        cp -f "$mode7" "$HOME/Library/Fonts/"
    else
        mkdir -p "$HOME/.local/share/fonts"
        cp -f "$bedstead" "$HOME/.local/share/fonts/"
        cp -f "$mode7" "$HOME/.local/share/fonts/"
        if have fc-cache; then
            fc-cache -f "$HOME/.local/share/fonts" >/dev/null 2>&1 || true
        fi
    fi
}

choose_compiler() {
    if [ -n "${CC:-}" ] && have "$CC"; then
        return
    fi

    if [ "$OS" = "Darwin" ] && have clang; then
        CC=clang
    elif have gcc; then
        CC=gcc
    elif have cc; then
        CC=cc
    else
        echo "ERROR: No C compiler found."
        exit 1
    fi

    export CC
}

compile_programs() {
    choose_compiler

    echo "Compiling C programs with ${CC}..."

    local srcdir="$SCRIPT_DIR/C/src"

    if [ ! -d "$srcdir" ]; then
        echo "ERROR: Source directory not found: $srcdir"
        exit 1
    fi

    "$CC" "$srcdir/imsai8080.c"   -o "$SCRIPT_DIR/imsai8080"
    "$CC" "$srcdir/school.c"      -o "$SCRIPT_DIR/school"
    "$CC" "$srcdir/dialer.c"      -o "$SCRIPT_DIR/dialer"
    "$CC" "$srcdir/pan-am.c"      -o "$SCRIPT_DIR/pan-am"
    "$CC" "$srcdir/bank.c"        -o "$SCRIPT_DIR/bank"
    "$CC" "$srcdir/wopr.c"        -o "$SCRIPT_DIR/wopr"
    "$CC" "$srcdir/tic-tac-toe.c" -o "$SCRIPT_DIR/tic-tac-toe"
    "$CC" "$srcdir/starwars.c"    -o "$SCRIPT_DIR/starwars"
}

install_llama_cpp() {
    echo "Installing local llama.cpp neural response module..."

    local llama_dir="$SCRIPT_DIR/llama.cpp"
    local server_bin="$llama_dir/build/bin/llama-server"
    local models_dir="$SCRIPT_DIR/models"
    local model_file="$models_dir/gemma-4-E4B-it-UD-Q4_K_XL.gguf"
    local model_url="https://huggingface.co/unsloth/gemma-4-E4B-it-GGUF/resolve/main/gemma-4-E4B-it-UD-Q4_K_XL.gguf?download=true"

    if [ ! -d "$llama_dir/.git" ]; then
        if [ -e "$llama_dir" ]; then
            echo "ERROR: $llama_dir exists but is not a llama.cpp git checkout." >&2
            exit 1
        fi

        echo "Cloning llama.cpp..."
        git clone --depth 1 https://github.com/ggml-org/llama.cpp.git "$llama_dir"
    else
        echo "llama.cpp already cloned; using existing checkout."
    fi

    echo "Building llama-server..."
    cmake -S "$llama_dir" -B "$llama_dir/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$llama_dir/build" --config Release --target llama-server -j 4

    if [ ! -x "$server_bin" ]; then
        echo "ERROR: llama-server was not built successfully: $server_bin" >&2
        exit 1
    fi

    mkdir -p "$models_dir"

    if [ -f "$model_file" ]; then
        echo "WOPR model already present: $model_file"
    else
        echo "Downloading WOPR model (Gemma 4 E4B Instruct UD-Q4_K_XL, approximately 5.1 GB)..."
        download_file "$model_url" "$model_file.part"
        mv "$model_file.part" "$model_file"
    fi

    echo "llama.cpp neural response module ready."
}

install_data_files() {
    echo "Installing data/config files..."

    # Preserve existing runtime .txt data files.
    if compgen -G "$SCRIPT_DIR/*.txt" > /dev/null; then
        echo "Existing .txt data files found; leaving them unchanged."
    else
        cp -f "$SCRIPT_DIR"/C/src/*.txt "$SCRIPT_DIR"/ 2>/dev/null || true
    fi

    if [ -f "$SCRIPT_DIR/C/src/imsai8080.json" ]; then
        cp -f "$SCRIPT_DIR/C/src/imsai8080.json" "$SCRIPT_DIR/"
    fi
}


verify_intranet() {
    echo "Checking local intranet..."

    if [ ! -f "$SCRIPT_DIR/intranet/index.html" ]; then
        echo "ERROR: Local intranet not found at:"
        echo "  $SCRIPT_DIR/intranet/index.html"
        echo
        echo "Ensure the intranet/ directory is present inside the Wargames folder."
        exit 1
    fi
}

make_scripts_executable() {
    echo "Marking shell scripts executable..."
    find "$SCRIPT_DIR" -maxdepth 1 -type f -name "*.sh" -exec chmod +x {} \;
}

case "$OS" in
    Darwin)
        install_macos_dependencies
        ;;
    Linux)
        install_linux_dependencies
        ;;
    *)
        echo "ERROR: Unsupported operating system: $OS"
        exit 1
        ;;
esac

install_cool_retro_term
install_vidtex
install_fonts
compile_programs
install_llama_cpp
install_data_files
verify_intranet
make_scripts_executable

echo
echo "Install complete."
echo "Run: ./imsai8080"
