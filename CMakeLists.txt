cmake_minimum_required(VERSION 4.0)
project(Hala_widowiskowo_sportowa)

set(CMAKE_CXX_STANDARD 20)

add_executable(Hala_widowiskowo_sportowa main.cpp

        include/common.h
        src/kibic.cpp
        src/pracownik_techniczny.cpp
        src/kasjer.cpp
        src/kierownik.cpp

)
