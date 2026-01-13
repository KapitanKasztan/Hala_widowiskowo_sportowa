//
// Created by kasztan on 12/4/25.
//
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>

//  == STALE ==:
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include "include/common.h"
#define K_KIBICOW 100
#define LICZBA_KAS 10
#define LICZBA_SEKTOROW 8
#define STANOWISKA_NA_SEKTOR 2
#define MAX_OSOB_NA_STANOWISKU 3
#define MAX_PRZEPUSZCZONYCH 5
#define DRUZYNA_A 0
#define DRUZYNA_B 1

typedef struct {
    int id;
    int druzyna;        // DRUZYNA_A lub DRUZYNA_B
    int sektor;         // 0-7
    int jest_vip;
    int jest_dzieckiem; // < 15 lat
    int id_opiekuna;    // ID opiekuna dla dzieci
    int ma_bilet;
} Kibic;

typedef struct {
    int liczba_osob;
    int druzyna_na_stanowisku;  // -1 jeśli puste
    int kibice_ids[MAX_OSOB_NA_STANOWISKU];
} Stanowisko;

typedef struct {
    Stanowisko stanowiska[STANOWISKA_NA_SEKTOR];
    int kolejka[200];           // ID kibiców w kolejce
    int druzyny_w_kolejce[200]; // Drużyny kibiców w kolejce
    int przepuszczeni[200];     // Ile razy kibic przepuścił innych
    int rozmiar_kolejki;
} WejscieDoSektora;

typedef struct {
    int kolejka_do_kasy;
    int sprzedane_bilety;
    int otwarte_kasy;
    int vip_w_kolejce;

    WejscieDoSektora wejscia[LICZBA_SEKTOROW];
    int kibice_na_hali;
    int czas_meczu;             // 0 = przed meczem, 1 = trwa
} Hala;
