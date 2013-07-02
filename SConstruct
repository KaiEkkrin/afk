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

if GetOption('dbg'):
    if GetOption('pg'):
        extra_ccflags = ['-g', '-pg']
        extra_ldflags = ['-pg']
        variant_dir = 'build/debug_pg'
    else:
        extra_ccflags = ['-g']
        extra_ldflags = []
        variant_dir = 'build/debug'
else:
    if GetOption('pg'):
        extra_ccflags = ['-O3', '-pg']
        extra_ldflags = ['-pg']
        variant_dir = 'build/release_pg'
    else:
        extra_ccflags = ['-O3']
        extra_ldflags = []
        variant_dir = 'build/release'

SConscript('src/SConscript', variant_dir=variant_dir, exports='extra_ccflags extra_ldflags')

