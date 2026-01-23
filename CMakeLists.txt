cmake_minimum_required(VERSION 4.0)
project(Hala_widowiskowo_sportowa)

set(CMAKE_CXX_STANDARD 20)

add_executable(Hala_widowiskowo_sportowa
        main.cpp
        include/common.h
        src/kierownik.cpp
        src/log_server.cpp
)

add_executable(kasjer
        src/kasjer.cpp
        include/common.h
)

add_executable(kibic_vip
        src/kibic_vip.cpp
        include/common.h
)

add_executable(kibic
        src/kibic.cpp
        include/common.h
)

add_executable(pracownik_techniczny
        src/pracownik_techniczny.cpp
        include/common.h
)

set_target_properties(Hala_widowiskowo_sportowa kasjer kibic_vip kibic pracownik_techniczny
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)