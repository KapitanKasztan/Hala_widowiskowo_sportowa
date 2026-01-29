// kibic_vip.cpp - Wersja zsynchronizowana
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include "../include/common.h"

void proces_kibica_vip(int idx, int shm_id, int sem_id, int msg_id) {
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) exit(1);

    Kibic *kibic_vip = &hala->kibice_vip[idx];
    kibic_vip->pid = getpid();

    // Wchodzi do kolejki VIP
    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kolejka_do_kasy_VIP[hala->rozmiar_kolejki_kasy_vip++] = idx;
    int pozycja = hala->rozmiar_kolejki_kasy_vip;
    sem_post_ipc(sem_id, SEM_MAIN);

    printf("[VIP %d] Czekam na bilet (Kolejka VIP poz: %d)\n", idx, pozycja);

    // ODBIÓR NA KANALE OFFSETOWANYM
    // Musi być zgodny z tym, co wysyła Kasjer
    long my_mtype = idx + VIP_MTYPE_OFFSET;

    struct moj_komunikat kom;
    if (msgrcv(msg_id, &kom, sizeof(kom) - sizeof(long), my_mtype, 0) == -1) {
        perror("msgrcv VIP");
        shmdt(hala);
        exit(1);
    }

    if (kom.akcja == 0) {
        printf("[VIP %d] Brak biletów VIP. Odchodzę!\n", idx);
        shmdt(hala);
        exit(0);
    }

    printf("[VIP %d] Otrzymałem bilet! Wchodzę na halę.\n", idx);

    // Wchodzi na halę (zwiększa licznik, na który czeka Main)
    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kibice_na_hali++;
    hala->kibice_w_sektorze[SEKTOR_VIP][hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]++] = idx;
    hala->liczba_kibicow_VIP++;
    kibic_vip->na_hali = 1;
    sem_post_ipc(sem_id, SEM_MAIN);

    printf("[VIP %d] Jestem w sektorze VIP!\n", idx);

    // Oglądanie meczu
    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        usleep(500000);
    }

    if (hala->ewakuacja) printf("[VIP %d] Ewakuacja!\n", idx);
    else printf("[VIP %d] Koniec meczu.\n", idx);

    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kibice_na_hali--;
    sem_post_ipc(sem_id, SEM_MAIN);

    shmdt(hala);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    proces_kibica_vip(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}