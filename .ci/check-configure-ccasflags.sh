#!/bin/sh

# Assembler defaults must include flags added after AM_PROG_AS, while explicit
# CCASFLAGS values (including empty ones) must remain under the caller's control.
set -eu

srcdir=$(CDPATH= cd "${1:-$(dirname "$0")/..}" && pwd)
workdir=$(mktemp -d "${TMPDIR:-/tmp}/libffi-ccasflags.XXXXXX")
trap 'rm -rf "$workdir"' 0
trap 'exit 1' HUP INT TERM

cat > "$workdir/query.mk" <<'EOF'
print-cflags:
	@printf '%s\n' '$(CFLAGS)'
print-ccasflags:
	@printf '%s\n' '$(CCASFLAGS)'
EOF

check_flags()
(
    name=$1
    unset CFLAGS CCASFLAGS
    expected=inherit
    set --
    case "$name" in
        default) ;;
        cflags)
            CFLAGS='-O0 -DLIBFFI_TEST_CFLAGS=1'
            export CFLAGS
            ;;
        environment)
            expected='-DLIBFFI_TEST_CCASFLAGS=1'
            CCASFLAGS=$expected
            export CCASFLAGS
            ;;
        environment-empty)
            expected=
            CCASFLAGS=
            export CCASFLAGS
            ;;
        command-line)
            expected='-DLIBFFI_TEST_CCASFLAGS=1'
            set -- "CCASFLAGS=$expected"
            ;;
        command-line-empty)
            expected=
            set -- CCASFLAGS=
            ;;
    esac

    mkdir "$workdir/$name" || exit 1
    cd "$workdir/$name" || exit 1
    if ! "$srcdir/configure" --disable-builddir --disable-docs "$@" \
        > configure.log 2>&1; then
        cat configure.log
        exit 1
    fi
    actual=$(make --no-print-directory -s -f Makefile -f "$workdir/query.mk" print-ccasflags) || exit 1
    if test "$expected" = inherit; then
        expected=$(make --no-print-directory -s -f Makefile -f "$workdir/query.mk" print-cflags) || exit 1
    fi
    if test "$actual" != "$expected"; then
        echo "FAIL: $name: expected CCASFLAGS='$expected', got '$actual'"
        exit 1
    fi

    # Exercise late make-time CFLAGS changes without requiring a cross compiler.
    # The default must follow CFLAGS, while an explicit override stays unchanged.
    case "$name" in
        default|cflags) expected='-O0 -DLIBFFI_TEST_LATE_CFLAGS=1' ;;
    esac
    actual=$(make --no-print-directory -s -f Makefile -f "$workdir/query.mk" \
        CFLAGS='-O0 -DLIBFFI_TEST_LATE_CFLAGS=1' print-ccasflags) || exit 1
    if test "$actual" != "$expected"; then
        echo "FAIL: $name: make override expected CCASFLAGS='$expected', got '$actual'"
        exit 1
    fi
    echo "PASS: $name"
)

status=0
for name in default cflags environment environment-empty command-line command-line-empty; do
    check_flags "$name" || status=1
done
exit "$status"
