// kierownik.cpp - Implementacja zgodna z dokumentacją SO (sygnały)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include "../include/common.h"

// Sygnały używane przez kierownika (zgodnie z dokumentacją)
#define SYGNAL_WSTRZYMAJ SIGUSR1  // sygnał1 - wstrzymaj wpuszczanie
#define SYGNAL_WZNOW     SIGUSR2  // sygnał2 - wznów wpuszczanie
#define SYGNAL_EWAKUACJA SIGRTMIN // sygnał3 - ewakuacja

static Hala *g_hala = NULL;
static int g_sem_id = -1;
static int g_msg_id = -1;
static volatile sig_atomic_t sektor_oproznieny[LICZBA_SEKTOROW] = {0};

// Obsługa potwierdzenia od pracownika technicznego (zgodnie z dokumentacją)
void obsluga_potwierdzenia(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    if (sig == SIGRTMIN + 1) {
        // Pracownik techniczny potwierdził oprÃ³Å¼nienie sektora
        int sektor = info->si_value.sival_int;
        if (sektor >= 0 && sektor < LICZBA_SEKTOROW) {
            sektor_oproznieny[sektor] = 1;
            printf("[KIEROWNIK] Sektor %d został oprÃ³Å¼niony\n", sektor);
        }
    }
}

void wstrzymaj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] BÅ‚Ä™dny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] Wstrzymuję wpuszczanie do sektora %d\n", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 1;
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    // WyÅ›lij sygnaÅ‚1 (SIGUSR1) do wszystkich pracowników danego sektora
    for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
        pid_t pracownik_pid = g_hala->wejscia[sektor].pracownik_pids[st];
        if (pracownik_pid > 0) {
            kill(pracownik_pid, SYGNAL_WSTRZYMAJ);
        }
    }
}

void wznow_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] BÅ‚Ä™dny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] Wznawiam wpuszczanie do sektora %d\n", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 0;
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    // WyÅ›lij sygnaÅ‚2 (SIGUSR2) do wszystkich pracowników danego sektora
    for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
        pid_t pracownik_pid = g_hala->wejscia[sektor].pracownik_pids[st];
        if (pracownik_pid > 0) {
            kill(pracownik_pid, SYGNAL_WZNOW);
        }
    }
}

void ewakuuj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] BÅ‚Ä™dny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] ZarzÄ…dzam EWAKUACJÄ˜ sektora %d\n", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 1;  // Najpierw wstrzymaj
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    sektor_oproznieny[sektor] = 0;

    // WyÅ›lij sygnaÅ‚3 (SIGRTMIN) do wszystkich pracowników danego sektora
    for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
        pid_t pracownik_pid = g_hala->wejscia[sektor].pracownik_pids[st];
        if (pracownik_pid > 0) {
            union sigval val;
            val.sival_int = sektor;
            sigqueue(pracownik_pid, SYGNAL_EWAKUACJA, val);
        }
    }
}

void ewakuuj_cala_hale() {
    printf("[KIEROWNIK] === EWAKUACJA CAÅEJ HALI ===\n");

    sem_wait_ipc(g_sem_id, SEM_MAIN);
    g_hala->ewakuacja = 1;
    sem_post_ipc(g_sem_id, SEM_MAIN);

    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        ewakuuj_sektor(i);
    }
}

int czy_sektor_oproznieny(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) return 0;
    return sektor_oproznieny[sektor];
}

int czy_wszystkie_sektory_oprozniaje() {
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        if (!sektor_oproznieny[i]) return 0;
    }
    return 1;
}

void proces_kierownika(int shm_id, int sem_id, int msg_id) {
    // Dołącz do pamięci dzielonej
    g_hala = (Hala*)shmat(shm_id, NULL, 0);
    if (g_hala == (void*)-1) {
        perror("shmat kierownik");
        exit(1);
    }

    g_sem_id = sem_id;
    g_msg_id = msg_id;

    // Ustaw obsługę potwierdzenia od pracowników (zgodnie z dokumentacją)
    struct sigaction sa;
    sa.sa_sigaction = obsluga_potwierdzenia;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN + 1, &sa, NULL);

    printf("[KIEROWNIK] Rozpoczynam pracę, PID: %d\n", getpid());

    // Zapisz PID kierownika w pamięci dzielonej
    g_hala->kierownik_pid = getpid();

    // Główna pętla kierownika
    int czas_pracy = 0;
    while (!g_hala->mecz_zakonczony && !g_hala->ewakuacja) {
        sem_wait_ipc(sem_id, SEM_MAIN);

        // Monitoruj sektory i podejmuj decyzje
        for (int s = 0; s < LICZBA_SEKTOROW; s++) {
            // Przykład: wstrzymaj sektor jeśli jest pełny
            if (g_hala->kibice_w_sektorze_ilosc[s] >= POJEMNOSC_SEKTORA) {
                if (!g_hala->wejscia[s].wstrzymane) {
                    sem_post_ipc(sem_id, SEM_MAIN);
                    wstrzymaj_sektor(s);
                    sem_wait_ipc(sem_id, SEM_MAIN);
                }
            }

            // Przykład: wznów sektor jeśli są wolne miejsca
            if (g_hala->kibice_w_sektorze_ilosc[s] < POJEMNOSC_SEKTORA * 0.9) {
                if (g_hala->wejscia[s].wstrzymane) {
                    sem_post_ipc(sem_id, SEM_MAIN);
                    wznow_sektor(s);
                    sem_wait_ipc(sem_id, SEM_MAIN);
                }
            }
        }

        // Symulacja incydentu wymagającego ewakuacji (rzadko)
        if (czas_pracy > 30 && (rand() % 1000) == 0) {
            printf("[KIEROWNIK] WYKRYTO ZAGROŻENIE! Zarządzam ewakuację!\n");
            sem_post_ipc(sem_id, SEM_MAIN);
            ewakuuj_cala_hale();
            break;
        }

        sem_post_ipc(sem_id, SEM_MAIN);
        sleep(1);
        czas_pracy++;
    }

    // Po meczu - ewakuacja
    if (g_hala->mecz_zakonczony && !g_hala->ewakuacja) {
        printf("[KIEROWNIK] Mecz zakończony - zarządzam opuszczenie hali\n");
        ewakuuj_cala_hale();

        // Czekaj na oprÃ³Å¼nienie wszystkich sektorów
        printf("[KIEROWNIK] Czekam na oprÃ³Å¼nienie wszystkich sektorów...\n");
        int timeout = 60;
        while (!czy_wszystkie_sektory_oprozniaje() && timeout > 0) {
            usleep(500000);
            timeout--;
        }

        if (czy_wszystkie_sektory_oprozniaje()) {
            printf("[KIEROWNIK] Wszystkie sektory oprÃ³Å¼nione\n");
        } else {
            printf("[KIEROWNIK] Timeout - nie wszystkie sektory oprÃ³Å¼nione\n");
        }
    }

    printf("[KIEROWNIK] Kończę pracę\n");
    shmdt(g_hala);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Użycie: %s <shm_id> <sem_id> <msg_id>\n", argv[0]);
        return 1;
    }

    int shm_id = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msg_id = atoi(argv[3]);

    proces_kierownika(shm_id, sem_id, msg_id);

    return 0;
}