// include/common.h - Wersja kuloodporna (Ignoruje błędy przy zamykaniu)
#pragma once

#include <errno.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define K_KIBICOW 160
#define LICZBA_KAS 10
#define LICZBA_SEKTOROW 8
#define POJEMNOSC_SEKTORA (K_KIBICOW / 8)
#define STANOWISKA_NA_SEKTOR 2
#define MAX_OSOB_NA_STANOWISKU 3
#define MAX_PRZEPUSZCZONYCH 5
#define DRUZYNA_A 0
#define DRUZYNA_B 1
#define POJEMNOSC_VIP (int)(0.1 * K_KIBICOW)
#define SEKTOR_VIP 8

#define LIMIT_SPRZEDAZY (K_KIBICOW + POJEMNOSC_VIP)
#define VIP_MTYPE_OFFSET (K_KIBICOW * 3)

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define SEM_MAIN 0
#define SEM_KASY 1
#define SEM_KOLEJKA 2
#define SEM_WEJSCIA 3
#define LICZBA_SEMAFOROW (3 + LICZBA_SEKTOROW + 1)

struct moj_komunikat {
    long int mtype;
    char text[256];
    int kibic_id;
    int sektor;
    int akcja;
};

#define MSG_BILET 1
#define MSG_KONTROLA 2
#define MSG_EWAKUACJA 3
#define MSG_STATUS 4

typedef struct {
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
    int id_towarzysza;
    int id_opiekuna_ref;
    pid_t pid;
} Kibic;

typedef struct {
    int liczba_osob;
    int druzyna_na_stanowisku;
    int kibice_ids[MAX_OSOB_NA_STANOWISKU];
} Stanowisko;

typedef struct {
    Stanowisko stanowiska[STANOWISKA_NA_SEKTOR];
    int kolejka_do_kontroli[K_KIBICOW * 4];
    int rozmiar_kolejki;
    int wstrzymane;
    int kibice_w_sektorze[K_KIBICOW];
    int kibice_w_sektorze_count;
    pid_t pracownik_pids[STANOWISKA_NA_SEKTOR];
} WejscieDoSektora;

typedef struct {
    key_t shm_key;
    key_t sem_key;
    key_t msg_key;
    int sem_id;
    int msg_id;

    int sprzedane_bilety;
    int otwarte_kasy;
    int sprzedane_bilety_w_sektorze[LICZBA_SEKTOROW + 1];

    int kolejka_do_kasy[K_KIBICOW];
    int kolejka_do_kasy_VIP[POJEMNOSC_VIP];
    int rozmiar_kolejki_kasy;
    int rozmiar_kolejki_kasy_vip;

    WejscieDoSektora wejscia[LICZBA_SEKTOROW];

    int kibice_na_hali;
    int kibice_w_sektorze[LICZBA_SEKTOROW+1][POJEMNOSC_SEKTORA];
    int kibice_w_sektorze_ilosc[LICZBA_SEKTOROW + 1];

    int dzieci_bez_opiekuna[K_KIBICOW / 10];
    int rozmiar_dzieci;

    Kibic kibice[K_KIBICOW + K_KIBICOW];
    int liczba_kibicow;
    Kibic kibice_vip[POJEMNOSC_VIP + 1];
    int liczba_kibicow_VIP;

    int czas_meczu;
    int mecz_rozpoczety;
    int mecz_zakonczony;
    int ewakuacja;

    pid_t kierownik_pid;
    pid_t kasy_pids[LICZBA_KAS];
    int shm_id;
} Hala;

// === NAPRAWIONE FUNKCJE SEMAFORÓW ===
// Ignorują błędy związane z usuwaniem semaforów (EINVAL, EIDRM)

static inline int sem_wait_ipc(int sem_id, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = -1;
    op.sem_flg = 0;
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue; // Przerwanie sygnałem - próbuj dalej
        if (errno == EINVAL || errno == EIDRM) return -1; // Semafor usunięty - ciche wyjście
        perror("semop wait");
        return -1;
    }
    return 0;
}

static inline int sem_post_ipc(int sem_id, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = 1;
    op.sem_flg = 0;
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        if (errno == EINVAL || errno == EIDRM) return -1; // Semafor usunięty - ciche wyjście
        perror("semop post");
        return -1;
    }
    return 0;
}

static inline void sem_init_ipc(int sem_id, int sem_num, int value) {
    union semun arg;
    arg.val = value;
    if (semctl(sem_id, sem_num, SETVAL, arg) == -1) perror("semctl SETVAL");
}