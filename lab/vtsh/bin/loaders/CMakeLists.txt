
add_executable(cpu-calc-crc cpu-calc-crc.c)
target_compile_definitions(cpu-calc-crc PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(ema-join-nl ema-join-nl.c)
target_compile_definitions(ema-join-nl PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(io-loader io-loader.c)
target_compile_definitions(io-loader PRIVATE _POSIX_C_SOURCE=200809L)
