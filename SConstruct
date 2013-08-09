AddOption(
    '--dbg',
    action='store_true',
    help='Debug build',
    default=False)

AddOption(
    '--pg',
    action='store_true',
    help='Enable profiling',
    default=False)

AddOption(
    '--llvm',
    action='store_true',
    help='Use LLVM',
    default=False)

extra_ccflags = []
extra_ldflags = []
extra_libs = []

if GetOption('dbg'):
    if GetOption('pg'):
        extra_ccflags = ['-g', '-pg']
        extra_ldflags = ['-pg']
        variant_dir = 'build/debug_pg'
    else:
        extra_ccflags = ['-g']
        extra_ldflags = []
        variant_dir = 'build/debug'
    extra_ccflags.append('-DAFK_GL_DEBUG=1')
else:
    if GetOption('pg'):
        extra_ccflags = ['-O3', '-pg', '-g']
        extra_ldflags = ['-pg']
        variant_dir = 'build/release_pg'
    else:
        extra_ccflags = ['-O3', '-g']
        extra_ldflags = []
        variant_dir = 'build/release'

if GetOption('llvm'):
    variant_dir += '_llvm'
    compiler = 'clang++'
else:
    compiler = 'g++'

# TODO Restrict this to Linux.
# On Windows, instead apply -DAFK_WGL.  On Mac, -DAFK_CGL or somesuch.
extra_ccflags.append('-DAFK_GLX')
extra_libs.append('-lX11')

SConscript('src/SConscript', variant_dir=variant_dir, exports='compiler extra_ccflags extra_ldflags extra_libs')

