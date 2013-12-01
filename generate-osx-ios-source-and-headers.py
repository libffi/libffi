#!/usr/bin/env python
import subprocess
import re
import os
import errno
import collections
import glob
import argparse

def get_sdkinfo(sdkname):
    ret = {}
    for line in subprocess.Popen(['xcodebuild', '-sdk', sdkname, '-version'], stdout=subprocess.PIPE).stdout:
      kv = line.strip().split(': ', 1)
      if len(kv) == 2:
          k, v = kv
          ret[k] = v
    return ret


class Platform(object):
    pass


class simulator_platform(Platform):
    arch = 'i386'
    prefix = "#ifdef __i386__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'x86'
    src_files = ['darwin.S', 'ffi.c']

    sdk = 'iphonesimulator'
    triple = 'i386-apple-darwin11'
    sdkinfo = get_sdkinfo(sdk)
    sdkroot = sdkinfo['Path']
    version_min = '-miphoneos-version-min=5.1.1'
    directory = 'ios'


class simulator64_platform(Platform):
    arch = 'x86_64'
    prefix = "#ifdef __x86_64__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'x86'
    src_files = ['darwin64.S', 'ffi64.c']

    sdk = 'iphonesimulator'
    triple = 'x86_64-apple-darwin13'
    sdkinfo = get_sdkinfo(sdk)
    sdkroot = sdkinfo['Path']
    version_min = '-miphoneos-version-min=7.0'
    directory = 'ios'


class device_platform(Platform):
    arch = 'armv7'
    prefix = "#ifdef __arm__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'arm'
    src_files = ['sysv.S', 'trampoline.S', 'ffi.c']

    sdk = 'iphoneos'
    triple = 'arm-apple-darwin11'
    sdkinfo = get_sdkinfo('iphoneos')
    sdkroot = sdkinfo['Path']
    version_min = '-miphoneos-version-min=5.1.1'
    directory = 'ios'


class device64_platform(Platform):
    arch = 'arm64'
    prefix = "#ifdef __arm64__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'aarch64'
    src_files = ['sysv.S', 'ffi.c']

    sdk = 'iphoneos'
    triple = 'aarch64-apple-darwin13'
    sdkinfo = get_sdkinfo('iphoneos')
    sdkroot = sdkinfo['Path']
    version_min = '-miphoneos-version-min=7.0'
    directory = 'ios'


class desktop32_platform(Platform):
    arch = 'i386'
    prefix = "#ifdef __i386__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'x86'
    src_files = ['darwin.S', 'ffi.c']

    sdk = 'macosx'
    triple = 'i386-apple-darwin11'
    sdkinfo = get_sdkinfo('macosx')
    sdkroot = sdkinfo['Path']
    version_min = '-mmacosx-version-min=10.7'
    directory = 'osx'


class desktop64_platform(Platform):
    arch = 'x86_64'
    prefix = "#ifdef __x86_64__\n\n"
    suffix = "\n\n#endif"
    src_dir = 'x86'
    src_files = ['darwin64.S', 'ffi64.c']

    sdk = 'macosx'
    triple = 'x86_64-apple-darwin11'
    sdkinfo = get_sdkinfo('macosx')
    sdkroot = sdkinfo['Path']
    version_min = '-mmacosx-version-min=10.7'
    directory = 'osx'


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST:
            pass
        else:
            raise


def move_file(src_dir, dst_dir, filename, file_suffix=None, prefix='', suffix=''):
    mkdir_p(dst_dir)
    out_filename = filename

    if file_suffix:
        split_name = os.path.splitext(filename)
        out_filename = "%s_%s%s" % (split_name[0], file_suffix, split_name[1])

    with open(os.path.join(src_dir, filename)) as in_file:
        with open(os.path.join(dst_dir, out_filename), 'w') as out_file:
            if prefix:
                out_file.write(prefix)

            out_file.write(in_file.read())

            if suffix:
                out_file.write(suffix)


def list_files(src_dir, pattern=None, filelist=None):
    if pattern: filelist = glob.iglob(os.path.join(src_dir, pattern))
    for file in filelist:
        yield os.path.basename(file)

def copy_files(src_dir, dst_dir, pattern=None, filelist=None, file_suffix=None, prefix=None, suffix=None):
    for filename in list_files(src_dir, pattern=pattern, filelist=filelist):
        move_file(src_dir, dst_dir, filename, file_suffix=file_suffix, prefix=prefix, suffix=suffix)


def copy_src_platform_files(platform):
    src_dir = os.path.join('src', platform.src_dir)
    dst_dir = os.path.join(platform.directory, 'src', platform.src_dir)
    copy_files(src_dir, dst_dir, filelist=platform.src_files, file_suffix=platform.arch, prefix=platform.prefix, suffix=platform.suffix)

def build_target(platform, platform_headers):
    def xcrun_cmd(cmd):
        return subprocess.check_output(['xcrun', '-sdk', platform.sdkroot, '-find', cmd]).strip()

    tag = "%s_%s" % (platform.sdk, platform.arch)
    build_dir = "build_%s" % tag
    mkdir_p(build_dir)
    env = dict(CC=xcrun_cmd('clang'),
               LD=xcrun_cmd('ld'),
               CFLAGS='-arch %s -isysroot %s %s' % (platform.arch, platform.sdkroot, platform.version_min))
    working_dir = os.getcwd()
    try:
        os.chdir(build_dir)
        subprocess.check_call(['../configure', '-host', platform.triple], env=env)
    finally:
        os.chdir(working_dir)

    for src_dir in [build_dir, os.path.join(build_dir, 'include')]:
        copy_files(src_dir,
                   os.path.join(platform.directory, 'include'),
                   pattern='*.h',
                   file_suffix=platform.arch,
                   prefix=platform.prefix,
                   suffix=platform.suffix)

        for filename in list_files(src_dir, pattern='*.h'):
            platform_headers[filename].add((platform.prefix, platform.arch, platform.suffix))


def make_tramp():
    with open('src/arm/trampoline.S', 'w') as tramp_out:
        p = subprocess.Popen(['bash', 'src/arm/gentramp.sh'], stdout=tramp_out)
        p.wait()


def generate_source_and_headers(generate_osx=True, generate_ios=True):
    copy_files('src', 'common/src', pattern='*.c')
    copy_files('include', 'common/include', pattern='*.h')

    if generate_ios:
        make_tramp()
        copy_src_platform_files(simulator_platform)
        copy_src_platform_files(simulator64_platform)
        copy_src_platform_files(device_platform)
        copy_src_platform_files(device64_platform)
    if generate_osx:
        copy_src_platform_files(desktop32_platform)
        copy_src_platform_files(desktop64_platform)

    platform_headers = collections.defaultdict(set)

    if generate_ios:
        build_target(simulator_platform, platform_headers)
        build_target(simulator64_platform, platform_headers)
        build_target(device_platform, platform_headers)
        build_target(device64_platform, platform_headers)
    if generate_osx:
        build_target(desktop32_platform, platform_headers)
        build_target(desktop64_platform, platform_headers)

    mkdir_p('common/include')
    for header_name, tag_tuples in platform_headers.iteritems():
        basename, suffix = os.path.splitext(header_name)
        with open(os.path.join('common/include', header_name), 'w') as header:
            for tag_tuple in tag_tuples:
                header.write('%s#include <%s_%s%s>\n%s\n' % (tag_tuple[0], basename, tag_tuple[1], suffix, tag_tuple[2]))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--only-ios', action='store_true', default=False)
    parser.add_argument('--only-osx', action='store_true', default=False)
    args = parser.parse_args()

    generate_source_and_headers(generate_osx=not args.only_ios, generate_ios=not args.only_osx)
