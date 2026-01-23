//
// Created by kasztan on 12/4/25.
//
#pragma once

#include <errno.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "logger.h"
#define K_KIBICOW 160                              // pojemność hali (podzielna przez 8 i 10)
#define LICZBA_KAS 10
#define LICZBA_SEKTOROW 8
#define POJEMNOSC_SEKTORA (K_KIBICOW / 8)
#define STANOWISKA_NA_SEKTOR 2
#define MAX_OSOB_NA_STANOWISKU 3
#define MAX_PRZEPUSZCZONYCH 5
#define DRUZYNA_A 0
#define DRUZYNA_B 1
#define POJEMNOSC_VIP (int)(0.1 * K_KIBICOW)     // mniej niż 0.3% * K
#define SEKTOR_VIP 8

class Kibic {
public:
    int id;
    int druzyna;
    int sektor;
    int jest_vip;
    int jest_dzieckiem;
    int id_opiekuna;
    int ma_bilet;
    int przepuscil;
    int liczba_biletow;
    int szuka_dziecka;
    int na_hali;
    sem_t bilet_sem;
    sem_t na_hali_sem;

    Kibic* towarzysz;
    Kibic* opiekun;

    Kibic() : id(0), druzyna(0), sektor(-1), jest_vip(0), jest_dzieckiem(0),
              id_opiekuna(-1), ma_bilet(0), przepuscil(0), liczba_biletow(0),
              szuka_dziecka(0), na_hali(0), towarzysz(nullptr), opiekun(nullptr) {}

    Kibic(int p_id, int p_druzyna, int p_sektor, int p_jest_vip, int p_ma_bilet,
          int p_przepuscil, int p_jest_dzieckiem, int p_id_opiekuna) :
        id(p_id), druzyna(p_druzyna), sektor(p_sektor), jest_vip(p_jest_vip),
        jest_dzieckiem(p_jest_dzieckiem), id_opiekuna(p_id_opiekuna),
        ma_bilet(p_ma_bilet), przepuscil(p_przepuscil), liczba_biletow(0),
        szuka_dziecka(0), na_hali(0), towarzysz(nullptr), opiekun(nullptr){
        if (sem_init(&bilet_sem, 0, 0) == -1) perror("sem_init");
        if (sem_init(&na_hali_sem, 0, 0) == -1) perror("sem_init");
    }


    Kibic(int p_id, int p_druzyna, int p_sektor) :
        id(p_id), druzyna(p_druzyna), sektor(p_sektor), jest_vip(0),
        jest_dzieckiem(0), id_opiekuna(-1), ma_bilet(1), przepuscil(0),
        liczba_biletow(1), szuka_dziecka(0), na_hali(0),
        towarzysz(nullptr), opiekun(nullptr) {
        if (sem_init(&na_hali_sem, 0, 0) == -1) perror("sem_init");

    }

    Kibic(int p_id, int VIP) :
        id(p_id), druzyna(0), sektor(SEKTOR_VIP), jest_vip(VIP),
        jest_dzieckiem(0), id_opiekuna(-1), ma_bilet(0), przepuscil(0),
        liczba_biletow(0), szuka_dziecka(0), na_hali(0),
        towarzysz(nullptr), opiekun(nullptr) {
        if (sem_init(&bilet_sem, 0, 0) == -1) perror("sem_init");
    }
};

typedef struct {
    int liczba_osob;
    bool druzyna_na_stanowisku;
    int kibice_ids[MAX_OSOB_NA_STANOWISKU];
} Stanowisko;

typedef struct {
    Stanowisko stanowiska[STANOWISKA_NA_SEKTOR];
    int kolejka_do_kontroli[K_KIBICOW/LICZBA_SEKTOROW];
    int rozmiar_kolejki;
    int wstrzymane;
    int kibice_w_sektorze[K_KIBICOW/LICZBA_SEKTOROW];
    sem_t sektor_sem;

} WejscieDoSektora;

typedef struct {
    // Kasy
    int sprzedane_bilety;
    int otwarte_kasy;
    int sprzedane_bilety_w_sektorze[LICZBA_SEKTOROW + 1];  // +1 dla VIP

    // Kolejka do kasy
    int kolejka_do_kasy[K_KIBICOW];
    int kolejka_do_kasy_VIP[POJEMNOSC_VIP];
    int rozmiar_kolejki_kasy;
    int rozmiar_kolejki_kasy_vip;

    // Wejścia i kontrola
    WejscieDoSektora wejscia[LICZBA_SEKTOROW];

    // Kibice na hali
    int kibice_na_hali;
    //int kibice_w_sektorze[LICZBA_SEKTOROW];
    int kibice_w_sektorze[LICZBA_SEKTOROW+1][POJEMNOSC_SEKTORA];

    int kibice_w_sektorze_ilosc[LICZBA_SEKTOROW + 1]; // +1 dla VIP

    // Dzieci bez opiekuna
    int dzieci_bez_opiekuna[K_KIBICOW / 10];
    int rozmiar_dzieci;

    // Wszyscy kibice
    Kibic kibice[K_KIBICOW + K_KIBICOW];  // +towarzysze
    int liczba_kibiców;

    Kibic kibice_vip[POJEMNOSC_VIP + 1];
    int liczba_kibiców_VIP;

    // Stan meczu
    int czas_meczu;
    int mecz_rozpoczety;
    int mecz_zakonczony;
    int ewakuacja;

    // semafory
    sem_t main_sem;
    int g_shm_id;
} Hala;

void proces_kasy(int id, Hala *hala);
void generator_kas(Hala *hala);
void proces_stanowiska(int sektor_id, int stanowisko_id, Hala *hala);
void proces_kibica_vip(int idx, Kibic *kibic, Hala *hala);
void proces_kibica_z_kontrola(int moj_idx, Kibic *kibic, Hala *hala);