// main.cpp - Pełna wersja z VIP-ami
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include "include/common.h"

volatile sig_atomic_t zakoncz = 0;
int g_shm_id = -1;
int g_sem_id = -1;
int g_msg_id = -1;

void obsluga_sygnalu(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[MAIN] Otrzymano sygnał %d - kończę symulację\n", sig);
        zakoncz = 1;
    }
}

void sprzatanie() {
    printf("[MAIN] Sprzątanie zasobów...\n");
    if (g_msg_id >= 0) msgctl(g_msg_id, IPC_RMID, NULL);
    if (g_sem_id >= 0) semctl(g_sem_id, 0, IPC_RMID);
    if (g_shm_id >= 0) shmctl(g_shm_id, IPC_RMID, NULL);
    printf("[MAIN] Zasoby zwolnione. Do widzenia!\n");
}

void wyswietl_status(Hala *hala) {
    sem_wait_ipc(hala->sem_id, SEM_MAIN);
    printf("\n=== STATUS ===\n");
    printf("Sprzedane bilety: %d/%d\n", hala->sprzedane_bilety, LIMIT_SPRZEDAZY);
    printf("Kibice na hali: %d\n", hala->kibice_na_hali);
    printf("Otwarte kasy: %d\n", hala->otwarte_kasy);
    printf("Kolejka do kasy: %d\n", hala->rozmiar_kolejki_kasy);
    printf("VIP w kolejce: %d\n", hala->rozmiar_kolejki_kasy_vip);
    printf("Bilety w sektorach: ");
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        printf("[%d:%d] ", i, hala->sprzedane_bilety_w_sektorze[i]);
    }
    printf("[VIP:%d]\n", hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]);
    printf("===============\n");
    sem_post_ipc(hala->sem_id, SEM_MAIN);
}

int main(int argc, char *argv[]) {
    int czas_tp = 5;
    if (argc > 1) czas_tp = atoi(argv[1]);

    srand(time(NULL));

    printf("===========================================\n");
    printf("=== SYMULACJA HALI Z VIP-ami ===\n");
    printf("===========================================\n");
    printf("Pojemność: %d + %d VIP\n", K_KIBICOW, POJEMNOSC_VIP);
    printf("Dynamiczny offset VIP: %d\n", VIP_MTYPE_OFFSET);

    struct sigaction sa;
    sa.sa_handler = obsluga_sygnalu;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    atexit(sprzatanie);

    key_t shm_key = ftok(".", 'S');
    key_t sem_key = ftok(".", 'M');
    key_t msg_key = ftok(".", 'Q');

    if (shm_key == -1 || sem_key == -1 || msg_key == -1) { perror("ftok"); return 1; }

    g_shm_id = shmget(shm_key, sizeof(Hala), IPC_CREAT | 0666);
    if (g_shm_id < 0) { perror("shmget"); return 1; }

    Hala *hala = (Hala*) shmat(g_shm_id, NULL, 0);
    if (hala == (void*)-1) { perror("shmat"); return 1; }

    g_sem_id = semget(sem_key, LICZBA_SEMAFOROW, IPC_CREAT | 0666);
    if (g_sem_id < 0) { perror("semget"); return 1; }

    sem_init_ipc(g_sem_id, SEM_MAIN, 1);
    sem_init_ipc(g_sem_id, SEM_KASY, 1);
    sem_init_ipc(g_sem_id, SEM_KOLEJKA, 1);
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        sem_init_ipc(g_sem_id, SEM_WEJSCIA + i, 1);
    }

    g_msg_id = msgget(msg_key, IPC_CREAT | 0666);
    if (g_msg_id < 0) { perror("msgget"); return 1; }

    memset(hala, 0, sizeof(Hala));
    hala->shm_key = shm_key;
    hala->sem_key = sem_key;
    hala->msg_key = msg_key;
    hala->shm_id = g_shm_id;
    hala->sem_id = g_sem_id;
    hala->msg_id = g_msg_id;

    // Inicjalizacja Zwykłych
    printf("[MAIN] Inicjalizacja kibiców...\n");
    for (int i = 0; i < K_KIBICOW; i++) {
        hala->kibice[i].id = i;
        hala->kibice[i].druzyna = i % 2;
        hala->kibice[i].sektor = -1;
        hala->kibice[i].jest_vip = 0;
        hala->kibice[i].jest_dzieckiem = (rand() % 100 < 10) ? 1 : 0; // Dzieci włączone
        hala->kibice[i].ma_bilet = 0;
        hala->kibice[i].na_hali = 0;
        hala->kibice[i].liczba_biletow = 0;
        hala->kibice[i].przepuscil = 0;
        hala->kibice[i].pid = 0;
    }

    // WAŻNE: Ustawiamy licznik na startową liczbę kibiców,
    // aby nowi towarzysze otrzymywali ID > 160 i nie nadpisywali pamięci!
    hala->liczba_kibicow = K_KIBICOW;

    // Inicjalizacja VIP - WŁĄCZONA
    for (int i = 0; i < POJEMNOSC_VIP; i++) {
        hala->kibice_vip[i].id = i;
        hala->kibice_vip[i].jest_vip = 1;
        hala->kibice_vip[i].druzyna = 0;
        hala->kibice_vip[i].sektor = SEKTOR_VIP;
        hala->kibice_vip[i].ma_bilet = 0;
        hala->kibice_vip[i].na_hali = 0;
        hala->kibice_vip[i].pid = 0;
    }

    printf("[MAIN] Uruchamiam kierownika...\n");
    if (fork() == 0) {
        char s1[16], s2[16], s3[16];
        sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
        execl("./kierownik", "kierownik", s1, s2, s3, NULL);
        exit(1);
    }

    printf("[MAIN] Uruchamiam kasy...\n");
    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            char id[16], s1[16], s2[16], s3[16];
            sprintf(id, "%d", i+1); sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
            execl("./kasjer", "kasjer", id, s1, s2, s3, NULL);
            exit(1);
        }
        hala->otwarte_kasy++;
    }

    printf("[MAIN] Uruchamiam stanowiska...\n");
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            if (fork() == 0) {
                char sek[16], sta[16], s1[16], s2[16], s3[16];
                sprintf(sek, "%d", s); sprintf(sta, "%d", st);
                sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
                execl("./pracownik_techniczny", "pracownik_techniczny", sek, sta, s1, s2, s3, NULL);
                exit(1);
            }
        }
    }

    sleep(1);

    printf("[MAIN] Generuję kibiców (%d)...\n", K_KIBICOW);
    for (int i = 0; i < K_KIBICOW && !zakoncz; i++) {
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i); sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
            execl("./kibic", "kibic", idx, s1, s2, s3, NULL);
            exit(1);
        }
        usleep(10000 + rand() % 10000);
    }

    // Generowanie VIP - WŁĄCZONE
    printf("[MAIN] Generuję VIP-ów (%d)...\n", POJEMNOSC_VIP);
    for (int i = 0; i < POJEMNOSC_VIP && !zakoncz; i++) {
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i); sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
            execl("./kibic_vip", "kibic_vip", idx, s1, s2, s3, NULL);
            exit(1);
        }
        usleep(100000 + rand() % 200000);
    }

    printf("\n[MAIN] Start meczu za %d s...\n", czas_tp);
    sleep(czas_tp);
    hala->mecz_rozpoczety = 1;

    int timeout = 180;
    while (!zakoncz && timeout > 0) {
        wyswietl_status(hala);
        sem_wait_ipc(hala->sem_id, SEM_MAIN);
        int na_hali = hala->kibice_na_hali;
        int sprzedane = hala->sprzedane_bilety;
        sem_post_ipc(hala->sem_id, SEM_MAIN);

        // Limit uwzględnia VIP (176)
        if (na_hali >= sprzedane && sprzedane >= LIMIT_SPRZEDAZY) {
            printf("[MAIN] Pełna frekwencja (%d/%d)! Kończę symulację.\n", na_hali, LIMIT_SPRZEDAZY);
            break;
        }
        sleep(3);
        timeout -= 3;
    }

    printf("\n=== PODSUMOWANIE ===\n");
    printf("Kibice na hali: %d\n", hala->kibice_na_hali);
    printf("Sprzedane: %d/%d\n", hala->sprzedane_bilety, LIMIT_SPRZEDAZY);

    kill(0, SIGTERM);
    while (wait(NULL) > 0);
    shmdt(hala);
    return 0;
}