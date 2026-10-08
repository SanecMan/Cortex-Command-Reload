project('zlib', 'c', default_options: ['c_std=c11'])

# Build with Meson's CRT instead of importing the legacy /MT archive.
# Preserve the compatibility API and runtime CPU dispatch of the VS project.
zlib_sources = files(
  'src/adler32.c', 'src/adler32_fold.c', 'src/chunkset.c',
  'src/compare256.c', 'src/compress.c', 'src/cpu_features.c',
  'src/crc32_braid.c', 'src/crc32_braid_comb.c', 'src/crc32_fold.c',
  'src/deflate.c', 'src/deflate_fast.c', 'src/deflate_huff.c',
  'src/deflate_medium.c', 'src/deflate_quick.c', 'src/deflate_rle.c',
  'src/deflate_slow.c', 'src/deflate_stored.c', 'src/functable.c',
  'src/infback.c', 'src/inflate.c', 'src/inftrees.c',
  'src/insert_string.c', 'src/insert_string_roll.c', 'src/slide_hash.c',
  'src/trees.c', 'src/uncompr.c', 'src/zutil.c',
  'src/gzlib.c', 'src/gzread.c', 'src/gzwrite.c',
)
zlib_args = ['-DZLIB_COMPAT', '-DWITH_GZFILEOP', '-DNO_FSEEKO',
             '-D_CRT_SECURE_NO_DEPRECATE', '-D_CRT_NONSTDC_NO_DEPRECATE']
if host_machine.system() == 'windows'
  zlib_args += ['-DZLIB_WINAPI']
  # crc32_braid_p.h defines N; include the SDK before that private macro.
  if meson.get_compiler('c').get_argument_syntax() == 'msvc'
    zlib_args += ['/FIwindows.h']
  endif
endif
if host_machine.cpu_family() in ['x86', 'x86_64']
  zlib_sources += files(
    'src/arch/x86/x86_features.c', 'src/arch/x86/slide_hash_avx2.c',
    'src/arch/x86/chunkset_avx2.c', 'src/arch/x86/compare256_avx2.c',
    'src/arch/x86/adler32_avx2.c', 'src/arch/x86/adler32_sse42.c',
    'src/arch/x86/insert_string_sse42.c', 'src/arch/x86/chunkset_sse2.c',
    'src/arch/x86/compare256_sse2.c', 'src/arch/x86/slide_hash_sse2.c',
    'src/arch/x86/adler32_ssse3.c', 'src/arch/x86/chunkset_ssse3.c',
    'src/arch/x86/crc32_pclmulqdq.c',
  )
  zlib_args += ['-DX86_FEATURES', '-DX86_SSE42', '-DX86_SSE42_CRC_INTRIN',
               '-DX86_SSE2', '-DX86_SSSE3', '-DX86_PCLMULQDQ_CRC']
endif
zlib_includes = include_directories('include', 'src')
zlib_library = static_library('zlibstatic', zlib_sources,
  include_directories:zlib_includes, c_args:zlib_args,
  override_options:['c_std=c11'], install:false)
lib_dep = declare_dependency(link_with:zlib_library,
  include_directories:include_directories('include'))
meson.override_dependency('zlib', lib_dep)

zlib_roundtrip = executable('zlib-roundtrip', 'tests/roundtrip.c',
  dependencies:lib_dep, c_args:['-DZLIB_WINAPI'],
  build_by_default:false, install:false)
test('zlib round-trip', zlib_roundtrip)
