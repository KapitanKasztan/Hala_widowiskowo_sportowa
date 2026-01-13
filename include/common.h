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

// == STAŁE ==
#define K_KIBICOW 160                              // pojemność hali (podzielna przez 8 i 10)
#define LICZBA_KAS 10
#define LICZBA_SEKTOROW 8
#define POJEMNOSC_SEKTORA (K_KIBICOW / 8)
#define STANOWISKA_NA_SEKTOR 2
#define MAX_OSOB_NA_STANOWISKU 3
#define MAX_PRZEPUSZCZONYCH 5
#define DRUZYNA_A 0
#define DRUZYNA_B 1
#define POJEMNOSC_VIP (int)(0.003 * K_KIBICOW)     // mniej niż 0.3% * K
#define SEKTOR_VIP 8

// == KIBIC ==
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
    int w_kontroli;
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
        szuka_dziecka(0), na_hali(0), towarzysz(nullptr), opiekun(nullptr) {}

    Kibic(int p_id, int p_druzyna, int p_sektor) :
        id(p_id), druzyna(p_druzyna), sektor(p_sektor), jest_vip(0),
        jest_dzieckiem(0), id_opiekuna(-1), ma_bilet(1), przepuscil(0),
        liczba_biletow(1), szuka_dziecka(0), na_hali(0),
        towarzysz(nullptr), opiekun(nullptr) {}

    Kibic(int p_id, int VIP) :
        id(p_id), druzyna(0), sektor(SEKTOR_VIP), jest_vip(VIP),
        jest_dzieckiem(0), id_opiekuna(-1), ma_bilet(0), przepuscil(0),
        liczba_biletow(0), szuka_dziecka(0), na_hali(0),
        towarzysz(nullptr), opiekun(nullptr) {}
};

// == STANOWISKO KONTROLI ==
typedef struct {
    int liczba_osob;
    int druzyna_na_stanowisku;  // -1 jeśli puste
    int kibice_ids[MAX_OSOB_NA_STANOWISKU];
} Stanowisko;

// == WEJŚCIE DO SEKTORA ==
typedef struct {
    Stanowisko stanowiska[STANOWISKA_NA_SEKTOR];
    int kolejka_do_kontroli[K_KIBICOW / 4];
    int rozmiar_kolejki;
    int wstrzymane;  // kierownik może wstrzymać wpuszczanie
} WejscieDoSektora;

// == HALA ==
typedef struct {
    // Kasy
    int sprzedane_bilety;
    int otwarte_kasy;
    int bilety_w_sektorze[LICZBA_SEKTOROW + 1];  // +1 dla VIP

    // Kolejka do kasy
    int kolejka_do_kasy[K_KIBICOW];
    int rozmiar_kolejki_kasy;
    int vip_w_kolejce;

    // Wejścia i kontrola
    WejscieDoSektora wejscia[LICZBA_SEKTOROW];

    // Kibice na hali
    int kibice_na_hali;
    int kibice_w_sektorze[LICZBA_SEKTOROW + 1];
    int kibice_na_hali_ids[LICZBA_SEKTOROW][POJEMNOSC_SEKTORA];
    int kibice_vip_na_hali_ids[POJEMNOSC_VIP + 1];

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
} Hala;

// == DEKLARACJE FUNKCJI ==
void proces_kasy(int id, Hala *hala, sem_t *sem);
void generator_kas(Hala *hala, sem_t *sem);
void proces_stanowiska(int sektor_id, int stanowisko_id, Hala *hala, sem_t *sem);
void proces_kibica_vip(int idx, Kibic *kibic, Hala *hala, sem_t *sem);
void proces_kibica_z_kontrola(int moj_idx, Kibic *kibic, Hala *hala, sem_t *sem);