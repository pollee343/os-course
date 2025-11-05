
add_executable(cpu-calc-crc cpu_calc_crc.c)
target_compile_definitions(cpu-calc-crc PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(ema-join-nl ema-join-nl.c)
target_compile_definitions(ema-join-nl PRIVATE _POSIX_C_SOURCE=200809L)
