#!/bin/sh

set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_root=$(mktemp -d "${TMPDIR:-/tmp}/libffi-libdir.XXXXXX")
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cat > "$test_root/cc" <<'EOF'
#!/bin/sh
for arg do
  if test "$arg" = -print-multi-os-directory; then
    echo ../lib64
    exit 0
  fi
done
exec gcc "$@"
EOF
chmod +x "$test_root/cc"

mkdir "$test_root/default" "$test_root/explicit"

(
  cd "$test_root/default"
  CC="$test_root/cc" "$srcdir/configure" --disable-builddir >/dev/null
)
grep -Fqx 'toolexeclibdir = ${libdir}/../lib64' \
  "$test_root/default/Makefile"

(
  cd "$test_root/explicit"
  CC="$test_root/cc" "$srcdir/configure" --disable-builddir \
    --libdir=/test/custom-lib >/dev/null
)
grep -Fqx 'libdir = /test/custom-lib' "$test_root/explicit/Makefile"
grep -Fqx 'toolexeclibdir = ${libdir}' "$test_root/explicit/Makefile"
