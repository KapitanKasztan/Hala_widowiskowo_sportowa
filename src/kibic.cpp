#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <pthread.h>
#include "../include/common.h"

struct ThreadArg {
    Hala* hala;
    int id_kibica;
    int id_glownego;
    int sektor;
    int msg_id;
    int sem_id;
    int role;
};

void* watek_towarzysza(void* arg) {
    struct ThreadArg* data = (struct ThreadArg*)arg;
    Hala* hala = data->hala;
    int my_id = data->id_kibica;

    Kibic *ja = &hala->kibice[my_id];
    ja->pid = getpid();

    if (data->role == 1) {
        printf("[Opiekun %d] Ide z dzieckiem %d, sektor %d\n",
               my_id, data->id_glownego, data->sektor);
    } else {
        printf("[Towarzysz %d] Ide z kibicem %d, sektor %d\n",
               my_id, data->id_glownego, data->sektor);
    }

    sem_wait_ipc(data->sem_id, SEM_WEJSCIA + data->sektor);
    WejscieDoSektora *wejscie = &hala->wejscia[data->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = my_id;
    sem_post_ipc(data->sem_id, SEM_WEJSCIA + data->sektor);

    struct moj_komunikat kom;
    if (msgrcv(data->msg_id, &kom, sizeof(kom) - sizeof(long), my_id + 1, 0) == -1) {
        perror("msgrcv watek");
        pthread_exit(NULL);
    }

    if (kom.akcja == MSG_KONTROLA) {
        ja->na_hali = 1;
        if (data->role == 1)
            printf("[Opiekun %d] Jestem na hali, sektor %d\n", my_id, data->sektor);
        else
            printf("[Towarzysz %d] Jestem na hali, sektor %d\n", my_id, data->sektor);
    }

    // mecz
    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        usleep(500000);
    }

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

    // kolejka do kasy
    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = idx;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post_ipc(sem_id, SEM_MAIN);

    printf("[Kibic %d] Kolejka do kasy (poz: %d)%s\n",
           kibic->id, pozycja_kasa, kibic->jest_dzieckiem ? " [DZIECKO]" : "");

    // odbior biletu
    struct moj_komunikat kom;
    if (msgrcv(msg_id, &kom, sizeof(kom) - sizeof(long), idx + 1, 0) == -1) {
        perror("msgrcv kibic");
        shmdt(hala);
        exit(1);
    }

    if (kom.akcja == 0) {
        printf("[Kibic %d] Brak biletow\n", kibic->id);
        shmdt(hala);
        exit(0);
    }

    printf("[Kibic %d] Biletow: %d, sektor: %d\n", kibic->id, kibic->liczba_biletow, kibic->sektor);

    pthread_t thread_towarzysz;
    int thread_created = 0;
    struct ThreadArg t_arg;

    // towarzysz
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
            t_arg.role = 0;

            if (pthread_create(&thread_towarzysz, NULL, watek_towarzysza, &t_arg) == 0) {
                thread_created = 1;
            }
        }
    }
    // opiekun
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
            t_arg.role = 1;

            if (pthread_create(&thread_towarzysz, NULL, watek_towarzysza, &t_arg) == 0) {
                thread_created = 1;
            }
        }
    }

    // wejscie
    sem_wait_ipc(sem_id, SEM_WEJSCIA + kibic->sektor);
    WejscieDoSektora *wejscie = &hala->wejscia[kibic->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = idx;
    sem_post_ipc(sem_id, SEM_WEJSCIA + kibic->sektor);

    if (msgrcv(msg_id, &kom, sizeof(kom) - sizeof(long), idx + 1, 0) == -1) {
        perror("msgrcv kibic kontrola");
    } else if (kom.akcja == MSG_KONTROLA) {
        kibic->na_hali = 1;
        printf("[Kibic %d] Jestem na hali, sektor %d\n", kibic->id, kibic->sektor);
    }

    // mecz
    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        usleep(500000);
    }

    sem_wait_ipc(sem_id, SEM_MAIN);
    hala->kibice_na_hali--;
    sem_post_ipc(sem_id, SEM_MAIN);

    if (thread_created) {
        pthread_join(thread_towarzysz, NULL);
        printf("[Kibic %d] Wychodzę z towarzyszem\n", idx);
    }

    shmdt(hala);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    proces_kibica(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}
