// kibic.cpp - Wersja na WĄTKACH (rozwiązuje problem synchronizacji towarzyszy)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <pthread.h>
#include "../include/common.h"

// Struktura przekazywana do wątku towarzysza/opiekuna
struct ThreadArg {
    Hala* hala;
    int id_kibica; // ID towarzysza/opiekuna
    int id_glownego; // ID tego, kto kupił bilet
    int sektor;
    int msg_id;
    int sem_id;
    int role; // 0 = towarzysz, 1 = opiekun
};

// Funkcja wątku towarzysza/opiekuna
void* watek_towarzysza(void* arg) {
    struct ThreadArg* data = (struct ThreadArg*)arg;
    Hala* hala = data->hala;
    int my_id = data->id_kibica;

    Kibic *ja = &hala->kibice[my_id];
    ja->pid = getpid(); // Wątek ma ten sam PID co proces główny

    if (data->role == 1) {
        printf("[Opiekun %d] (Wątek) Opiekuję się dzieckiem %d, idę do sektora %d\n",
               my_id, data->id_glownego, data->sektor);
    } else {
        printf("[Towarzysz %d] (Wątek) Idę z kibicem %d do sektora %d\n",
               my_id, data->id_glownego, data->sektor);
    }

    // Ustawienie się w kolejce do wejścia
    sem_wait_ipc(data->sem_id, SEM_WEJSCIA + data->sektor);
    WejscieDoSektora *wejscie = &hala->wejscia[data->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = my_id;
    sem_post_ipc(data->sem_id, SEM_WEJSCIA + data->sektor);

    // Oczekiwanie na wpuszczenie (na unikalnym kanale ID+1, nie PID)
    struct moj_komunikat kom;
    if (msgrcv(data->msg_id, &kom, sizeof(kom) - sizeof(long), my_id + 1, 0) == -1) {
        perror("msgrcv watek");
        pthread_exit(NULL);
    }

    if (kom.akcja == MSG_KONTROLA) {
        ja->na_hali = 1;
        if (data->role == 1)
            printf("[Opiekun %d] Jestem na hali w sektorze %d!\n", my_id, data->sektor);
        else
            printf("[Towarzysz %d] Jestem na hali w sektorze %d!\n", my_id, data->sektor);
    }

    // Symulacja oglądania meczu
    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        usleep(500000);
    }

    // Wyjście
    sem_wait_ipc(data->sem_id, SEM_MAIN);
    hala->kibice_na_hali--;
    sem_post_ipc(data->sem_id, SEM_MAIN);

    pthread_exit(NULL);
}

void proces_kibica(int idx, int shm_id, int sem_id, int msg_id) {
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) exit(1);

    Kibic *kibic = &hala->kibice[idx];
    kibic->pid = getpid();

    // 1. Kolejka do kasy
    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = idx;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post_ipc(sem_id, SEM_MAIN);

    printf("[Kibic %d] Czekam w kolejce do kasy (pozycja: %d)%s\n",
           kibic->id, pozycja_kasa, kibic->jest_dzieckiem ? " [DZIECKO]" : "");

    // 2. Odbiór biletu (kanał = idx + 1)
    struct moj_komunikat kom;
    if (msgrcv(msg_id, &kom, sizeof(kom) - sizeof(long), idx + 1, 0) == -1) {
        perror("msgrcv kibic");
        shmdt(hala);
        exit(1);
    }

    if (kom.akcja == 0) {
        printf("[Kibic %d] Brak biletów. Odchodzę!\n", kibic->id);
        shmdt(hala);
        exit(0);
    }

    printf("[Kibic %d] Mam %d bilet(y), idę do sektora %d\n",
           kibic->id, kibic->liczba_biletow, kibic->sektor);

    // 3. Uruchamianie wątku towarzysza/opiekuna
    pthread_t thread_towarzysz;
    int thread_created = 0;
    struct ThreadArg t_arg;

    // A) Towarzysz (2 bilety)
    if (!kibic->jest_dzieckiem && kibic->liczba_biletow == 2) {
        sem_wait_ipc(sem_id, SEM_MAIN);
        int id_towarzysza = kibic->id_towarzysza;
        sem_post_ipc(sem_id, SEM_MAIN);

        if (id_towarzysza > 0) {
            t_arg.hala = hala;
            t_arg.id_kibica = id_towarzysza;
            t_arg.id_glownego = idx;
            t_arg.sektor = kibic->sektor;
            t_arg.msg_id = msg_id;
            t_arg.sem_id = sem_id;
            t_arg.role = 0; // Towarzysz

            if (pthread_create(&thread_towarzysz, NULL, watek_towarzysza, &t_arg) == 0) {
                thread_created = 1;
            }
        }
    }
    // B) Opiekun (Dziecko ma 2 bilety logicznie, 1 fizycznie w tej implementacji kasjera, ale sprawdzamy relację)
    else if (kibic->jest_dzieckiem) {
        sem_wait_ipc(sem_id, SEM_MAIN);
        int id_opiekuna = kibic->id_opiekuna_ref;
        sem_post_ipc(sem_id, SEM_MAIN);

        if (id_opiekuna > 0) {
            t_arg.hala = hala;
            t_arg.id_kibica = id_opiekuna;
            t_arg.id_glownego = idx;
            t_arg.sektor = kibic->sektor;
            t_arg.msg_id = msg_id;
            t_arg.sem_id = sem_id;
            t_arg.role = 1; // Opiekun

            if (pthread_create(&thread_towarzysz, NULL, watek_towarzysza, &t_arg) == 0) {
                thread_created = 1;
            }
        }
    }

    // 4. Główny kibic idzie do wejścia
    sem_wait_ipc(sem_id, SEM_WEJSCIA + kibic->sektor);
    WejscieDoSektora *wejscie = &hala->wejscia[kibic->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = idx;
    sem_post_ipc(sem_id, SEM_WEJSCIA + kibic->sektor);

    // Czekaj na wpuszczenie (kanał idx + 1)
    if (msgrcv(msg_id, &kom, sizeof(kom) - sizeof(long), idx + 1, 0) == -1) {
        perror("msgrcv kibic kontrola");
    } else if (kom.akcja == MSG_KONTROLA) {
        kibic->na_hali = 1;
        printf("[Kibic %d] Jestem na hali w sektorze %d!\n", kibic->id, kibic->sektor);
    }

    // Oglądaj mecz
    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        usleep(500000);
    }

    // Wyjście
    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kibice_na_hali--;
    sem_post_ipc(sem_id, SEM_MAIN);

    // WAŻNE: Poczekaj na wątek towarzysza
    if (thread_created) {
        pthread_join(thread_towarzysz, NULL);
        printf("[Kibic %d] Wychodzę razem z towarzyszem/opiekunem.\n", idx);
    }

    shmdt(hala);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    proces_kibica(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}