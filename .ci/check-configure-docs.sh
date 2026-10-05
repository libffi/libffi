#!/bin/sh

# Check the documentation option using the generated Makefile's recursion list.
set -eu

srcdir=$(CDPATH= cd "${1:-$(dirname "$0")/..}" && pwd)
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' 0
trap 'exit 1' HUP INT TERM

cat > "$workdir/query.mk" <<'EOF'
print-subdirs:
	@printf '%s\n' '$(SUBDIRS)'
EOF

check_docs()
(
    name=$1
    expected=$2
    shift 2
    mkdir "$workdir/$name" || exit 1
    cd "$workdir/$name" || exit 1
    if ! "$srcdir/configure" "$@" > configure.log 2>&1; then
        cat configure.log
        exit 1
    fi
    subdirs=$(make --no-print-directory -s -f Makefile -f "$workdir/query.mk" print-subdirs) || exit 1
    case " $subdirs " in
        *" doc "*) actual=yes ;;
        *) actual=no ;;
    esac
    if test "$actual" != "$expected"; then
        echo "FAIL: $name: expected docs=$expected, got docs=$actual"
        exit 1
    fi
    echo "PASS: $name: docs=$actual"
)

status=0
check_docs default yes || status=1
check_docs enable yes --enable-docs || status=1
check_docs enable-yes yes --enable-docs=yes || status=1
check_docs disable no --disable-docs || status=1
check_docs enable-no no --enable-docs=no || status=1
exit "$status"
