cmake_minimum_required(VERSION 4.0)
project(Hala_widowiskowo_sportowa)

set(CMAKE_CXX_STANDARD 20)

# 1. Znajdź bibliotekę wątków (to odpowiada za flagę -pthread)
find_package(Threads REQUIRED)

add_executable(Hala_widowiskowo_sportowa
        main.cpp
        include/common.h
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
# 2. Linkowanie biblioteki wątków do pliku kibic (tutaj jest używana)
target_link_libraries(kibic PRIVATE Threads::Threads)

add_executable(pracownik_techniczny
        src/pracownik_techniczny.cpp
        include/common.h
)

add_executable(kierownik
        src/kierownik.cpp
        include/common.h
)

#add_executable(
#        tests
#        tests.cpp
#        include/common.h
#)
# Uwaga: Dodałem też 'kierownik' do listy poniżej, bo w Twoim kodzie go brakowało
set_target_properties(Hala_widowiskowo_sportowa kasjer kibic_vip kibic pracownik_techniczny kierownik
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)