AddOption(
    '--dbg',
    action='store_true',
    help='Debug build',
    default=False)

AddOption(
    '--odbg',
    action='store_true',
    help='Optimised build with debug symbols',
    default=False)

AddOption(
    '--opencl11',
    action='store_true',
    help='Use only OpenCL 1.1 calls',
    default=False)

AddOption(
    '--pg',
    action='store_true',
    help='Enable profiling',
    default=False)

extra_ccflags = []
extra_ldflags = []
extra_libs = []

if GetOption('dbg'):
    extra_ccflags += ['-g', '-DAFK_GL_DEBUG=1']
    variant_dir = 'build/debug'
elif GetOption('odbg'):
    extra_ccflags += ['-O3', '-g', '-DAFK_GL_DEBUG=1']
    variant_dir = 'build/odbg'
else:
    extra_ccflags += ['-O3']
    variant_dir = 'build/release'

if GetOption('pg'):
    extra_ccflags += ['-pg']
    extra_ldflags += ['-pg']
    variant_dir += '_pg'

if GetOption('opencl11'):
    extra_ccflags += ['-DAFK_OPENCL11']

# TODO Restrict this to Linux.
# On Windows, instead apply -DAFK_WGL.  On Mac, -DAFK_CGL or somesuch.
extra_ccflags += ['-DAFK_GLX']
extra_libs += ['-lX11']

SConscript('src/SConscript', variant_dir=variant_dir, exports='extra_ccflags extra_ldflags extra_libs')

