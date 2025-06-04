#!/bin/bash
set -x

if [[ $RUNNER_OS != 'Linux' ]]; then
    brew update --verbose
    # brew update > brew-update.log 2>&1
    # fix an issue with libtool on travis by reinstalling it
    brew uninstall libtool;
    brew install automake libtool dejagnu;

    # Download and extract the rlgl client
    wget -qO - https://rl.gl/cli/rlgl-darwin-amd64.tgz | \
	      tar --strip-components=2 -xvzf - ./rlgl/rlgl;

else
    # Download and extract the rlgl client
    case $HOST in
	      aarch64-*linux-gnu)
	          wget -qO - https://rl.gl/cli/rlgl-linux-arm.tgz | \
		            tar --strip-components=2 -xvzf - ./rlgl/rlgl;
	          ;;
	      ppc64le-linux-gnu)
	          wget -qO - https://rl.gl/cli/rlgl-linux-ppc64le.tgz | \
		            tar --strip-components=2 -xvzf - ./rlgl/rlgl;
	          ;;
	      s390x-linux-gnu)
	          wget -qO - https://rl.gl/cli/rlgl-linux-s390x.tgz | \
		            tar --strip-components=2 -xvzf - ./rlgl/rlgl;
	          ;;
	      *)
	          wget -qO - https://rl.gl/cli/rlgl-linux-amd64.tgz | \
		            tar --strip-components=2 -xvzf - ./rlgl/rlgl;
	          ;;
    esac

    sudo apt-get clean # clear the cache
    sudo apt-get update
    sudo apt install libltdl-dev zip

    shopt -s inherit_errexit   # propagate failure out of subshells (bash >= 4.4)

    URL=https://ftpmirror.gnu.org/autoconf/autoconf-2.72.tar.gz
    MAX_TRIES=5
    BACKOFF=2                  # seconds; will double every attempt

    attempt=1
    while (( attempt <= MAX_TRIES )); do
        echo "➡️  Attempt ${attempt}/${MAX_TRIES}"
        if wget --retry-connrefused \
                --waitretry=1 \
                --read-timeout=20 \
                --timeout=15 \
                -qO-  "$URL" \
                | tar -xzv; then
            echo "✅  Success on attempt ${attempt}"
            break
        fi

        echo "⚠️  Download or extract failed, retrying in ${BACKOFF}s…"
        sleep "$BACKOFF"
        BACKOFF=$(( BACKOFF * 2 ))
        (( attempt++ ))
    done

    if (( attempt > MAX_TRIES )); then
        echo "❌  Exhausted retries ($MAX_TRIES) – aborting." >&2
        exit 1
    fi

    mkdir -p ~/i
    (cd autoconf-2.72; ./configure --prefix=$HOME/i; make; make install)

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
