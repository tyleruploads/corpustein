#!/usr/bin/env bash

set -euo pipefail

readonly c_REPOSITORY_URL="https://github.com/tyleruploads/corpustein.git"
readonly c_TAR_URL="https://github.com/tyleruploads/corpustein/releases/latest/download/corpustein-x86_64.tar.gz"
readonly c_TAR_NAME=$(basename "$c_TAR_URL")

readonly COLOR_RESET="\033[0m"
readonly COLOR_BOLD="\033[1m"
readonly COLOR_RED="\033[31m"
readonly COLOR_GREEN="\033[32m"
readonly COLOR_BLUE="\033[34m"
readonly COLOR_CYAN="\033[36m"
readonly COLOR_YELLOW="\033[33m"
readonly COLOR_ORANGE="\033[38;2;255;165;0m"

readonly VERSION=$(curl -s https://api.github.com/repos/tyleruploads/corpustein/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

if [[ -z "$VERSION" ]]; then
    echo -e "${COLOR_RED}ERROR: Could not determine the latest Corpustein version. Check your internet connection status and access to the GitHub API.${COLOR_RESET}" >&2
    exit 1
fi

case "${1:-}" in
    --version|-v)
        echo "Target Version: Corpustein ${VERSION}+dev"
        exit 0
        ;;
    --help|-h)
        echo "Usage: ./setup.sh [OPTION]"
        echo ""
        echo "Options:"
        echo "    -v, --version     Show version information"
        echo "    -h, --help        Show this help message"
        echo ""
        echo "Running without options starts interactive installer menu"
        exit 0
        ;;
    -*)
        echo -e "${COLOR_RED}Error: Unknown option '$1'${COLOR_RESET}"
        echo -e "${COLOR_RED}Run './setup.sh --help' for usage.${COLOR_RESET}"
        exit 1
        ;;
esac

print_div() {
    local char="${1:-=}"

    printf '%*s\n' "$(tput cols)" '' | tr ' ' "$char"
}

trap 'echo -e "\n${COLOR_RED}Installation aborted.${COLOR_RESET}"; exit 1' INT TERM

clear

print_div
echo -e "${COLOR_BLUE}${COLOR_BOLD}Corpustein Installer${COLOR_RESET}"
echo -e "${COLOR_ORANGE}Corpustein is a blazingly fast N-gram Markov Chain model with a configurable order and a rolling window system.\nIt is coded only in the C++20 Standard Library, which makes it small and fast.\nIt even uses an alternate terminal buffer to make it look sleek and beautiful.${COLOR_RESET}"
print_div
echo -e "${COLOR_GREEN}Maintainer     : Tyler N. (tyleruploads)${COLOR_RESET}"
echo -e "${COLOR_GREEN}Repository     : https://github.com/tyleruploads/corpustein${COLOR_RESET}"
echo -e "${COLOR_GREEN}Target Version : ${VERSION}+dev${COLOR_RESET}"
echo -e "${COLOR_GREEN}Script Source  : https://github.com/tyleruploads/corpustein/blob/main/setup.sh${COLOR_RESET}"
echo -e "${COLOR_GREEN}License        : Open Source (MIT) ${COLOR_RESET}"
print_div

if [[ "$OSTYPE" == "darwin"* ]]; then
    echo -e "${COLOR_RED}Error: Installer currently not supported on macOS${COLOR_RESET}" >&2
    exit 1
fi

if ! sudo -n true 2>/dev/null; then
    echo -e "${COLOR_YELLOW}This script is not currently running with administrative rights. You will be asked to authorize administrative privileges if necessary.${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}If the script fails with any sort of permission error, rerun it with sudo, even if you already provided the administrator password before the failure.${COLOR_RESET}"
    sleep 2
fi

options=("Install Remote Binary" "Compile Locally and Install" "Uninstall Corpustein" "Exit")
PS3="Please enter your choice (1-4): "

echo -e "\n"

select opt in "${options[@]}"; do
    case $opt in
        "Install Remote Binary")
            echo -e "${COLOR_CYAN}Downloading tar.gz from GitHub Releases${COLOR_RESET}"

            if ! curl -fsSL --retry 5 --retry-delay 2 -o "$c_TAR_NAME" "$c_TAR_URL"; then
                echo -e "${COLOR_RED}Error: Download failed entirely (network, 404, or GitHub error)${COLOR_RESET}" >&2
                exit 1
            fi

            if [ ! -s "$c_TAR_NAME" ]; then
                echo -e "${COLOR_RED}Error: Download resulted in an empty file.${COLOR_RESET}" >&2
                rm -f "$c_TAR_NAME"
                exit 1
            fi

            echo -e "${COLOR_CYAN}Unzipping tar.gz${COLOR_RESET}"

            sudo rm -rf corpustein
            mkdir -p corpustein
            tar -xzvf ${c_TAR_NAME} -C corpustein
            cd corpustein

            echo -e "${COLOR_CYAN}Installing Corpustein (sudo required)${COLOR_RESET}"

            sudo mkdir -p /usr/local/bin
            sudo rm -rf /usr/local/bin/corpustein
            sudo mv corpustein.x86_64 /usr/local/bin/corpustein
            chmod +x /usr/local/bin/corpustein &> /dev/null || sudo chmod +x /usr/local/bin/corpustein

            sudo rm -rf /usr/local/share/corpustein
            sudo mkdir /usr/local/share/corpustein
            sudo mv corpora /usr/local/share/corpustein/corpora
            
            LINE='export PATH="/usr/local/bin:$PATH"'

            TARGET_BASHRC="${SUDO_USER:+$(getent password "$SUDO_USER" | cut -d: -f6)}/.bashrc"
            [ -z "${SUDO_USER:-}" ] && TARGET_BASHRC="$HOME/.bashrc"

            if ! grep -qxF "$LINE" "$TARGET_BASHRC"; then
                echo "$LINE" >> "$TARGET_BASHRC"
                echo "${COLOR_GREEN}/usr/local/bin has been added to ~/.bashrc${COLOR_RESET}"
            fi

            echo -e "${COLOR_GREEN}Corpustein binary. has been successfully installed. You can run it at any time with the corpustein command.${COLOR_RESET}"
            echo -e "${COLOR_GREEN}To update your shell, run: source ~/.bashrc${COLOR_RESET}"

            ;;
        "Compile Locally and Install")
            echo -e "${COLOR_CYAN}[1/4] Installing dependencies${COLOR_RESET}\n"

            if command -v apt-get &>/dev/null; then
                echo -e "${COLOR_BOLD}Using apt-get...${COLOR_RESET}"
                sudo apt-get update -y
                sudo apt-get install -y build-essential git qt6-base-dev qt6-base-dev-tools cmake
            elif command -v dnf &>/dev/null; then
                echo -e "${COLOR_BOLD}Using dnf...${COLOR_RESET}"
                sudo dnf groupinstall -y "Development Tools"
                sudo dnf install -y git cmake qt6-qtbase-devel
            elif command -v pacman &>/dev/null; then
                echo -e "${COLOR_BOLD}Using pacman...${COLOR_RESET}"
                sudo pacman -S --noconfirm --needed base-devel git cmake qt6-base xcb-util-cursor
            elif command -v zypper &>/dev/null; then
                echo -e "${COLOR_BOLD}Using zypper...${COLOR_RESET}"
                sudo zypper refresh
                sudo zypper install -y -t pattern devel_basis
                sudo zypper install -y git cmake qt6-base-devel
            else
                echo -e "${COLOR_RED}Error: No supported package manager (apt-get, dnf, pacman, zypper) found.${COLOR_RESET}"
                exit 1
            fi

            echo -e "${COLOR_CYAN}[2/4] Cloning GitHub Repository${COLOR_RESET}\n"

            if [ -d ".git" ] && [[ "$(git config --get remote.origin.url)" == "$c_REPOSITORY_URL" ]]; then
                echo -e "${COLOR_BOLD}Already inside corpustein repository. Updating...${COLOR_RESET}"
                git pull
            else
                if [ -d "corpustein" ]; then
                    echo -e "${COLOR_BOLD}Directory corpustein already exists. Updating...${COLOR_RESET}"
                    cd "corpustein"
                    git pull
                else
                    echo -e "${COLOR_BOLD}Cloning corpustein repository${COLOR_RESET}"
                    git clone "$c_REPOSITORY_URL"
                    cd "corpustein"
                fi
            fi

            echo -e "${COLOR_CYAN}[3/4] Compiling Corpustein${COLOR_RESET}\n"

            rm -rf build # Removes build directory in case it already exists to prevent errors
            mkdir -p build

            cmake -B build -DCMAKE_BUILD_TYPE=Release
            cmake --build build --parallel

            echo -e "${COLOR_CYAN}[4/4] Installing Corpustein${COLOR_RESET}\n"
            sudo cmake --install build

            echo -e "${COLOR_GREEN}Compilation successful! Corpustein can be ran with the 'corpustein' command in the terminal.${COLOR_RESET}"
            ;;
        "Uninstall Corpustein")
            c_BINARY_PATH=$(command -v corpustein || true)

            if [ -n "$c_BINARY_PATH" ]; then
                echo -e "${COLOR_CYAN}Corpustein binary found at: $c_BINARY_PATH. Uninstalling...${COLOR_RESET}"
                sudo rm -f "$c_BINARY_PATH"
                echo -e "${COLOR_GREEN}Corpustein binary successfully uninstalled.${COLOR_RESET}"
            else
                if [ -f "/usr/local/bin/corpustein" ]; then
                    echo -e "${COLOR_CYAN}Corpustein binary found at: /usr/local/bin/corpustein. Uninstalling...${COLOR_RESET}"
                    sudo rm -f "/usr/local/bin/corpustein"
                    echo -e "${COLOR_CYAN}Corpustein binary successfully uninstalled.${COLOR_RESET}"
                else
                    echo -e "${COLOR_RED}Corpustein binary not found. Not uninstalling binary.${COLOR_RESET}" >&2
                fi
            fi

            if [ -d "/usr/local/share/corpustein" ]; then
                echo -e "${COLOR_CYAN}Corpustein files found at: /usr/local/share/corpustein. Deleting...${COLOR_RESET}"
                sudo rm -rf /usr/local/share/corpustein
                echo -e "${COLOR_GREEN}Corpustein files successfully deleted.${COLOR_RESET}"
            else
                echo -e "${COLOR_RED}No corpustein files found at /usr/local/share/corpustein. Not deleting.${COLOR_RESET}" >&2
            fi

            echo -e "${COLOR_BOLD}${COLOR_GREEN}To update your shell, run: source ~/.bashrc${COLOR_RESET}"

            ;;
        "Exit")
            break
            ;;
        *)
            echo -e "${COLOR_RED}Invalid option.${COLOR_RESET}"
            ;;
    esac
done



