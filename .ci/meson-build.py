#!/usr/bin/env python3

import os
import argparse
import subprocess
import re
import shutil
from pathlib import Path
from typing import List, Dict, Optional

host = os.environ.get('HOST', '')

def install_tools():
    # Try installing ninja from distro if possible
    if not shutil.which('ninja'):
        sudo = shutil.which('sudo')
        sudo = [sudo] if sudo else []
        if shutil.which('apt-get'):
            subprocess.check_call(sudo + ['apt-get', '-y', 'install', 'python3-pip', 'ninja-build'])
        elif shutil.which('dnf'):
            subprocess.run(sudo + ['dnf', '-y', 'install', 'python3-pip', 'ninja-build'])
    # Fallback to build samurai from source, it is a clone of ninja, much faster to build.
    if not shutil.which('ninja') or 'cygdrive' in shutil.which('ninja'):
        subprocess.check_call(['curl', '-sLO', 'https://github.com/michaelforney/samurai/releases/download/1.2/samurai-1.2.tar.gz'])
        subprocess.check_call(['tar', 'xzf', 'samurai-1.2.tar.gz'])
        subprocess.check_call(['make', '-C', 'samurai-1.2'])
        os.environ['NINJA'] = os.path.join(os.getcwd(), 'samurai-1.2', 'samu')
    # Install Meson from pypi if needed
    if not shutil.which('meson'):
        subprocess.check_call(['pip3', 'install', 'meson'])

def print_logs(fname: str):
    log_file = Path('builddir', 'meson-logs', fname)
    if log_file.exists():
        logs = log_file.read_text(encoding='utf-8')
        print(f'::group::==== {fname} ====')
        print(logs)
        print('::endgroup::')

def build(options: List[str], skip_tests: bool, ignore_tests_errors: bool):
    # Configure, build, and run unit tests
    try:
        subprocess.check_call(['meson', 'setup', 'builddir'] + options)
        subprocess.check_call(['meson', 'compile', '-C', 'builddir'])
    except:
        print_logs('meson-log.txt')
        raise
    try:
        if not skip_tests:
            subprocess.check_call(['meson', 'test', '-C', 'builddir', '--no-rebuild'])
    except:
        print_logs('meson-log.txt')
        print_logs('testlog.txt')
        if not ignore_tests_errors:
            raise

def generate_cross_file(template: Path) -> Path:
    cpu = host.split('-')[0]
    cpu_family = cpu
    if re.match('i.86', cpu):
        cpu_family = 'x86'
    elif cpu == 'armv7a':
        cpu_family = 'arm'
    content = template.read_text()
    content = content.replace('@HOST@', host)
    content = content.replace('@CPU@', cpu)
    content = content.replace('@CPU_FAMILY@', cpu_family)
    content = content.replace('@ANDROID_NDK_ROOT@', os.environ.get('ANDROID_NDK_ROOT', ''))
    content = content.replace('@ANDROID_API_LEVEL@', os.environ.get('ANDROID_API_LEVEL', ''))
    content = content.replace('@CC@', str(os.environ.get('CC', '').split()))
    content = content.replace('@CXX@', str(os.environ.get('CXX', '').split()))
    cross_file = Path('cross.txt')
    cross_file.write_text(content)
    print(f'::group::==== {cross_file} ====')
    print(content)
    print('::endgroup::')
    return cross_file

def run_in_docker(image: str, cross_file: Optional[Path] = None):
    cmd = ['docker', 'run', '--rm', '-t', '-v', f'{os.getcwd()}:/opt', '--workdir', '/opt']

    env = {}
    env['QEMU_LD_PREFIX'] = f'/usr/{host}'
    for k in {'QEMU_CPU', 'LIBFFI_TEST_OPTIMIZATION'}:
        try:
            env[k] = os.environ[k]
        except KeyError:
            pass
    for k, v in env.items():
        cmd += ['-e', f'{k}={v}']

    cmd += [image, '/opt/.ci/meson-build.sh']
    if cross_file:
        cmd += ['--cross-file', str(cross_file)]
    print('Run in docker:', cmd)
    subprocess.check_call(cmd)

def main() -> int:
    options = ['--default-library=both']
    skip_tests = False
    cross_file = None

    parser = argparse.ArgumentParser()
    parser.add_argument("--ignore-tests-errors", action='store_true')
    parser.add_argument("--cross-file")
    args = parser.parse_args()

    if host == 'moxie-elf':
        options.append('--default-library=static')
        options.append('--cross-file=.ci/meson-cross-moxie.txt')
    elif 'android' in host:
        cross_file = generate_cross_file(Path('.ci/meson-cross-android.txt'))
        options.append('--cross-file=' + str(cross_file))
        skip_tests = True
    elif host == 'arm32v7-linux-gnu':
        run_in_docker('quay.io/moxielogic/arm32v7-ci-build-container:latest')
        return 0
    elif host in ['m68k-linux-gnu', 'alpha-linux-gnu', 'sh4-linux-gnu']:
        gcc_options = ' -mcpu=547x' if host == 'm68k-linux-gnu' else ''
        os.environ['CC'] = f'{host}-gcc-8{gcc_options}'
        os.environ['CXX'] = f'{host}-g++-8{gcc_options}'
        cross_file = generate_cross_file(Path('.ci/meson-cross.txt'))
        run_in_docker('quay.io/moxielogic/cross-ci-build-container:latest', cross_file)
        return 0
    elif host in ['bfin-elf', 'm32r-elf', 'or1k-elf', 'powerpc-eabisim']:
        gcc_options = ' -msim' if host == 'bfin-elf' else ''
        os.environ['CC'] = f'{host}-gcc{gcc_options}'
        os.environ['CXX'] = f'{host}-g++{gcc_options}'
        cross_file = generate_cross_file(Path('.ci/meson-cross.txt'))
        run_in_docker(f'quay.io/moxielogic/libffi-ci-{host}', cross_file)
        return 0

    configure_options = os.environ.get('CONFIGURE_OPTIONS', '')
    if '--disable-shared' in configure_options:
        options.append('--default-library=static')

    optimization = os.environ.get('LIBFFI_TEST_OPTIMIZATION')
    if optimization:
        options.append('-Dtests_optimizations=' + ','.join(optimization.split()))

    if args.cross_file:
        options += ['--cross-file', args.cross_file]

    install_tools()
    build(options, skip_tests, args.ignore_tests_errors)
    if cross_file:
        cross_file.unlink()
    return 0

if __name__ == '__main__':
    exit(main())
