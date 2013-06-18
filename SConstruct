AddOption(
    '--dbg',
    action='store_true',
    help='Debug build',
    default=False)

env = Environment()
if GetOption('dbg'):
    extra_ccflags = ['-g']
    variant_dir = 'build/debug'
else:
    extra_ccflags = ['-O3']
    variant_dir = 'build/release'

SConscript('src/SConscript', variant_dir=variant_dir, duplicate=False, exports='extra_ccflags')

