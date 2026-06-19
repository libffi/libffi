#!/bin/bash
set -x

# Download the self-contained rlgl client from its GitHub releases into ./rlgl.
# rlgl runs on *this* runner (not the build target), so select the binary by
# the runner's own OS/arch.  Use a direct, version-pinned asset URL rather than
# the GitHub API: the unauthenticated api.github.com is rate-limited (60/hr/IP)
# and 403s under the CI matrix load, whereas release-asset downloads are not.
RLGL_VERSION="${RLGL_VERSION:-2.0.4}"
download_rlgl() {
    local uname_s uname_m os arch url
    uname_s=$(uname -s); uname_m=$(uname -m)
    case "$uname_s" in
        Darwin) os=darwin ;;
        *)      os=linux ;;
    esac
    case "$uname_m" in
        x86_64|amd64)  arch=amd64 ;;
        aarch64|arm64) arch=arm64 ;;
        *) echo "No rlgl binary available for $uname_s/$uname_m"; return 1 ;;
    esac
    url="https://github.com/atgreen/red-light-green-light/releases/download/v${RLGL_VERSION}/rlgl-${RLGL_VERSION}-${os}-${arch}.tar.gz"
    echo "Downloading rlgl: $url"
    curl -fsSL --retry 3 "$url" | tar -xzf - rlgl
    chmod +x rlgl
}

if [[ $RUNNER_OS != 'Linux' ]]; then
    brew update --verbose
    # brew update > brew-update.log 2>&1
    # fix an issue with libtool on travis by reinstalling it
    brew uninstall libtool;
    brew install automake libtool dejagnu gcc@15;

    # Download the rlgl client (macOS build path evaluates results with it)
    download_rlgl

else
    # Download the rlgl client for the runner's architecture
    download_rlgl

    sudo apt-get clean # clear the cache
    sudo apt-get update
    sudo apt install libltdl-dev zip

    case $HOST in
	      mips64el-linux-gnu | sparc64-linux-gnu)
        ;;
	      alpha-linux-gnu | arm32v7-linux-gnu | m68k-linux-gnu | sh4-linux-gnu)
	          sudo apt-get install qemu-user-static
	          ;;
	      hppa-linux-gnu )
	          sudo apt-get install -y qemu-user-static g++-5-hppa-linux-gnu
	          ;;
	      i386-pc-linux-gnu)
	          sudo apt-get install gcc-multilib g++-multilib;
	          ;;
	      moxie-elf)
	          echo 'deb [trusted=yes] https://repos.moxielogic.org:7114/MoxieLogic moxiedev main' | sudo tee -a /etc/apt/sources.list
	          sudo apt-get clean # clear the cache
	          sudo apt-get update ## -qq
	          sudo apt-get update
	          sudo apt-get install -y --allow-unauthenticated moxielogic-moxie-elf-gcc moxielogic-moxie-elf-gcc-c++ moxielogic-moxie-elf-gcc-libstdc++ moxielogic-moxie-elf-gdb-sim texinfo sharutils texlive dejagnu
	          ;;
	      x86_64-w64-mingw32)
	          sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 wine;
	          ;;
	      i686-w32-mingw32)
	          sudo apt-get install gcc-mingw-w64-i686 g++-mingw-w64-i686 wine;
	          ;;
    esac
    case $HOST in
	      arm32v7-linux-gnu)
        # don't install host tools
        ;;
	      *)
	          sudo apt-get install dejagnu texinfo sharutils
	          ;;
    esac
fi
