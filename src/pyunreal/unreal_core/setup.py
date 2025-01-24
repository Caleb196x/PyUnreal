from setuptools import setup, Extension
import os
import sys
import platform

# Get platform info
is_windows = platform.system() == "Windows" 
is_linux = platform.system() == "Linux"
is_mac = platform.system() == "Darwin"

# Set include directories
include_dirs = [
    "../../capnp/include"
]

# Set library directories
if is_windows:
    library_dirs = [
        "../../capnp/libs/Win64/Release"
    ]
elif is_linux:
    library_dirs = [
        "../../capnp/libs/Linux/Release"
    ]
elif is_mac:
    library_dirs = [
        "../../capnp/libs/Mac/Release"
    ]

# Set libraries to link based on platform
libraries = []
if is_windows:
    libraries.extend(['Advapi32', 'Secur32', 'Ws2_32', 'capnp', 'capnpc',  'capnp-rpc',  'capnp-json', 
                      'capnp-websocket', 'kj', 'kj-http', 'kj-async', 'kj-test'])
elif is_linux:
    libraries.extend(['capnp', 'capnpc',  'capnp-rpc',  'capnp-json', 'capnp-websocket', 'kj', 'kj-http', 'kj-async', 'kj-test'])
elif is_mac:
    libraries.extend(['capnp', 'capnpc',  'capnp-rpc',  'capnp-json', 'capnp-websocket', 'kj', 'kj-http', 'kj-async', 'kj-test'])

# Set compiler flags
extra_compile_args = []

if is_windows:
    extra_compile_args.extend([
        '/EHsc',      # Enable C++ exception handling
        '/GR',        # Enable RTTI
        '/MD',        # Use dynamic runtime library
        '/std:c++17'  # Use C++17 standard
    ])
else:  # Linux and macOS
    extra_compile_args.extend([
        '-std=c++17',
        '-frtti',     # Enable RTTI
        '-fexceptions'  # Enable exception handling
    ])

# Define the extension module
unreal_core = Extension(
    'unreal_core',
    sources=['unreal_core_cpython.cpp', 'ue_core.capnp.cpp'],
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    language='c++'
)

# Setup configuration
setup(
    name='unreal_core',
    version='0.0.1',
    description='Python bindings for Unreal Core rpc framework',
    ext_modules=[unreal_core]
)
