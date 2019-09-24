# Copyright (c) Mathias Kaerlev 2013-2014.
#
# This file is part of cuwo.
#
# cuwo is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# cuwo is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with cuwo.  If not, see <http://www.gnu.org/licenses/>.

import os
import sys
sys.path.append(os.path.dirname(__file__))
import numpy
from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize
from distutils.command import build_ext as _build_ext
import multiprocessing.pool
from distutils import log
from distutils.sysconfig import get_config_vars
from distutils import spawn
import subprocess
import platform
from cuwo.download import download_dependencies

# config_vars = get_config_vars()

# # suppress warnings
# IGNORE_FLAGS = ('-Wstrict-prototypes', '-mno-fused-madd')


# def filter_flags(name):
#     if name not in config_vars:
#         return
#     flags = config_vars[name].split()
#     for flag in flags[:]:
#         if flag in IGNORE_FLAGS:
#             flags.remove(flag)
#             continue
#         if flag in ('-Os', '-O2', '-O1'):
#             flags.append('-O3')
#             flags.remove(flag)
#             continue
#     config_vars[name] = ' '.join(flags)

# filter_flags('OPT')
# filter_flags('CFLAGS')
# filter_flags('CXXFLAGS')
# filter_flags('ARCHFLAGS')

macros = []
undef_macros = []
compile_args = []
link_args = []
ext_modules = []

names = [
    'cuwo.bytes',
    'cuwo.entity',
    'cuwo.tgen_wrap'
]

includes = ['./cuwo',
            './terraingen/tgen2/src',
            './terraingen/tgen2/external',
            numpy.get_include()]

libraries = []

if os.name == 'nt':
    names.append('cuwo.win32c')
    macros += [('_CRT_SECURE_NO_WARNINGS', None),
               ('WIN32', 1)]
    # compile_args.append('/std:c++11')
    compile_args.append('/std:c++11')
    compile_args.append('-Zi')
    # compile_args.append('/Od')
    # undef_macros.append('NDEBUG')
    # link_args.append('-debug')
    libraries.append('advapi32')
else:
    compile_args.append('-std=c++11')
    compile_args.append('-std=gnu99')
    compile_args.append('-fpermissive')

has_sse2 = False
if platform.machine() in ('AMD64', 'x86', 'x86_64', 'i386', 'i686'):
    print('Using SSE2 optimizations')
    macros += [('ENABLE_SSE2', None)]
    has_sse2 = True

tgen_sources = [
    './terraingen/tgen2/src/convert.cpp',
    './terraingen/tgen2/src/rpmalloc.c',
    './terraingen/tgen2/src/mem.cpp',
    './terraingen/tgen2/src/sqlite3.c',
    './terraingen/tgen2/src/tgen.cpp',
    './terraingen/tgen2/external/undname/undname.c',
    './terraingen/tgen2/external/pe-parse/parser-library/buffer.cpp',
    './terraingen/tgen2/external/pe-parse/parser-library/parse.cpp'
]

ext_args = dict(language='c++', include_dirs=includes,
                extra_compile_args=compile_args,
                extra_link_args=link_args,
                define_macros=macros, undef_macros=undef_macros,
                libraries=libraries)

tgen_module = Extension('cuwo.tgen', ['./cuwo/tgen.pyx'] + tgen_sources,
                        **ext_args)
ext_modules.append(tgen_module)

for name in names:
    use_ext_args = ext_args.copy()
    if os.name == 'nt' and name == 'cuwo.win32c':
        use_ext_args['libraries'] = libraries + ['winmm']

    ext_modules.append(Extension(name, ['./%s.pyx' % name.replace('.', '/')],
                                 **use_ext_args))

# class build_ext(_build_ext.build_ext):
#     def build_extensions(self):
#         self.check_extensions_list(self.extensions)

#         for ext in self.extensions:
#             ext.extra_compile_args += ["-Zi", "/Od"]
#             ext.extra_link_args += ["-debug"]
#             self.build_extension(ext)

setup(
    name='cuwo extensions',
    ext_modules=cythonize(ext_modules),
    # cmdclass={"build_ext": build_ext}
)
