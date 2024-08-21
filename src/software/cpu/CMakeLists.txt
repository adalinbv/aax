
set(CPU_HEADERS
  arch2d_simd.h
  arch3d_simd.h
)

set(CPU_SOURCES
  arch2d_cpu.c
  arch2d_neon.c
  arch2d_neon64.c
  arch2d_vfpv4.c
  arch2d_sse2.c
  arch2d_sse3.c
  arch2d_sse4.c
  arch2d_sse_vex.c
  arch2d_avx.c
  arch2d_fma3.c

  arch3d_neon.c
  arch3d_neon64.c
  arch3d_vfpv4.c
  arch3d_sse.c
  arch3d_sse2.c
  arch3d_sse3.c
  arch3d_sse_vex.c
  arch3d_avx.c
  arch3d_fma3.c

  rbuf_effects.c
  rbuf_limiter.c
  rbuf_limiter_tables.c
  rbuf_mixmulti.c
  rbuf_mixsingle.c
  render_monolithic.c
  render_threadpool.c
  waveforms.c
)

set(SOURCES "")
foreach(src ${CPU_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

