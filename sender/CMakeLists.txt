cmake_minimum_required(VERSION 3.16)

add_executable(fortxxi-sender
    main.c
    proc_stat.h
    proc_stat.c
#    send_loadavg.c
#    parse_proc_stat.c
)

target_link_libraries(fortxxi-sender m)
