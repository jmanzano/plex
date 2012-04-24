#!/usr/bin/env python
# encoding: utf-8
"""
bootstrap.py

Created by Jamie Kirkpatrick on 2011-01-15.
Copyright (c) 2011 Plex Inc. All rights reserved.
"""
import optparse
import subprocess
import sys
import os


VERBOSE=False
SDK="10.6"


class BootstrapError(RuntimeError):
    '''Internal exception: stores cmd output for debugging'''

    def __init__(self, msg, output):
        super(BootstrapError, self).__init__(msg)
        self.output = output


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'


def run_cmd(args, message = None, stderr = False, **kwargs):
    '''Run a command and capture the output iteratively'''
    if isinstance(args, str):
        args = [args]
    if message:
        print bcolors.OKBLUE + ("-> %s" % message) + bcolors.ENDC
    env = get_exe_environ() if "env" not in kwargs else kwargs["env"]
    cwd = kwargs.get("cwd", None)
    cmd = subprocess.Popen(args, env = env, cwd = cwd,
                           stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    output = ''
    while True:
        out = cmd.stdout.read(1)
        if out == '' and cmd.poll() != None:
            break
        if out != '':
            if VERBOSE:
                sys.stdout.write(out)
            else:
                output += out
    if cmd.wait() != 0:
        raise BootstrapError("Command failed: \"%s\"" % " ".join(args), output)


def get_exe_environ():
    env_path = get_env_path()
    env = os.environ
    local_env = {
        'PATH': "%s/../toolchain/bin:%s/bin:/usr/bin:/usr/sbin:/bin:/sbin" % (
            env_path, env_path),
        'CC':'clang',
        'CXX':'clang++',
        'PREFIX_PATH': env_path,
        'CFLAGS': "-I%s/include" % env_path,
        'CXXFLAGS': "-I%s/include" % env_path,
        'ACLOCAL': "aclocal -I \"%s/share/aclocal\"" % env_path,
        'LDFLAGS': "-L%s/lib" % env_path
        }
    env.update(local_env)
    return env


def get_env_path():
    '''Returns the path to the build environ'''
    script_path = os.path.abspath(os.path.dirname(__file__))
    return os.path.join(script_path, "vendor", "osx-%s_i386" % SDK)


def update_submodules():
    '''Update the registered git submodules'''
    args = ['git', 'submodule', 'update', '--init']
    run_cmd(args, "Updating submodules", env = None)


def configure_internal_libs(options):
    '''Configure the internal vendor libraries'''
    clean = ['find', '.', '-name', 'config.cache', '-exec', 'rm', '{}', ';']
    run_cmd(clean, "Cleaning caches")
    run_cmd(["./bootstrap"], "Bootstrapping internal libs")
    configure = ['./configure', '--with-arch=i386', '--with-ffmpeg-cc=' + options.ffmpegcc]
    if not options.debug:
        configure.append('--disable-debug')
    run_cmd(configure, "Configuring internal libs")
    if not options.noclean:
        run_cmd(['make', 'clean'])


def build_internal_libs():
    '''Build the internal libraries'''
    run_cmd(['make', 'xcode_depends'], "Building internal libs")


def bootstrap_dependencies():
    root_dir = os.path.abspath(".")
    working_dir = os.path.join(root_dir, "tools", "darwin", "depends")
    vendor_dir = os.path.join(root_dir, "vendor")
    os.system("cd %s && ./bootstrap" % working_dir)
    run_cmd(["./configure",
             "--with-staging=%s" % vendor_dir,
             "--with-darwin=osx",
             "--with-sdk=%s" % SDK],
            "Configuring vendor dependencies",
            cwd = working_dir)
    run_cmd("make", "Building vendor dependencies", cwd = working_dir)


def process_args():
    '''Process command line arguments'''
    global VERBOSE
    parser = optparse.OptionParser()
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose', default=False, help='Increase the chatter')
    parser.add_option('-d', '--debug', action='store_true', dest='debug', default=False, help='Make a debug build')
    parser.add_option('-c', '--configure-only', action='store_true', dest='confonly', default=False, help="Don't build, just configure")
    parser.add_option('-n', '--no-clean', action='store_true', dest='noclean', default=False, help="Don't clean caches, just build")
    parser.add_option('-f', '--ffmpeg-cc', action='store', type='string', dest='ffmpegcc', default='clang', help='Compiler to use for FFmpeg')
    (options, args) = parser.parse_args()
    VERBOSE = options.verbose

    return options

def build_rtmp():
  run_cmd(['make', '-C', 'lib/librtmp'], "Building librtmp")
  run_cmd(['make', '-C', 'lib/librtmp', 'install'], "Installing librtmp")
  run_cmd('./lib/librtmp/darwin_package_librtmp.sh', "Installing librtmp into system")
  
def main():
    try:
        options = process_args()
        update_submodules()
        bootstrap_dependencies()
        configure_internal_libs(options)
        if not options.confonly:
            build_internal_libs()
        build_rtmp()
    except BootstrapError, e:
        print bcolors.FAIL + ("...%s" % e) + bcolors.ENDC
        print "Output: %s" % e.output
        exit(1)
    except KeyboardInterrupt:
        print bcolors.FAIL + ("...interupted") + bcolors.ENDC
        exit(1)


if __name__ == '__main__':
    main()
