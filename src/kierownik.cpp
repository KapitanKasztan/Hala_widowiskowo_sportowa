#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include "../include/common.h"
#include "../include/logger.h"

#define SYGNAL_WSTRZYMAJ SIGUSR1
#define SYGNAL_WZNOW     SIGUSR2
#define SYGNAL_EWAKUACJA SIGRTMIN

static Hala *g_hala = NULL;
static int g_sem_id = -1;
static int g_msg_id = -1;
static volatile sig_atomic_t sektor_oproznieny[LICZBA_SEKTOROW] = {0};
static Logger *g_reporter = NULL;

void obsluga_potwierdzenia(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    if (sig == SIGRTMIN + 1) {
        int sektor = info->si_value.sival_int;
        if (sektor >= 0 && sektor < LICZBA_SEKTOROW) {
            sektor_oproznieny[sektor] = 1;
            reporter_info(g_reporter, "Potwierdzenie oproznienia sektora %d", sektor);
        }
    }
}

void wstrzymaj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        reporter_error(g_reporter, "Bledny sektor: %d", sektor);
        return;
    }

    reporter_info(g_reporter, "Wstrzymanie sektora %d", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 1;
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
        pid_t pracownik_pid = g_hala->wejscia[sektor].pracownik_pids[st];
        if (pracownik_pid > 0) {
            kill(pracownik_pid, SYGNAL_WSTRZYMAJ);
        }
    }
}

void wznow_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        reporter_error(g_reporter, "Bledny sektor: %d", sektor);
        return;
    }

    reporter_info(g_reporter, "Wznowienie sektora %d", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 0;
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
        pid_t pracownik_pid = g_hala->wejscia[sektor].pracownik_pids[st];
        if (pracownik_pid > 0) {
            kill(pracownik_pid, SYGNAL_WZNOW);
        }
    }
}

void ewakuuj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        reporter_error(g_reporter, "Bledny sektor: %d", sektor);
        return;
    }

    reporter_warning(g_reporter, "Ewakuacja sektora %d", sektor);

    sem_wait_ipc(g_sem_id, SEM_WEJSCIA + sektor);
    g_hala->wejscia[sektor].wstrzymane = 1;
    sem_post_ipc(g_sem_id, SEM_WEJSCIA + sektor);

    sektor_oproznieny[sektor] = 0;

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
    reporter_critical(g_reporter, "Ewakuacja calej hali");

    sem_wait_ipc(g_sem_id, SEM_MAIN);
    g_hala->ewakuacja = 1;
    sem_post_ipc(g_sem_id, SEM_MAIN);

    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        ewakuuj_sektor(i);
    }

    reporter_info(g_reporter, "Polecenia ewakuacji wyslane");
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
    g_hala = (Hala*)shmat(shm_id, NULL, 0);
    if (g_hala == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    g_sem_id = sem_id;
    g_msg_id = msg_id;

    g_reporter = reporter_init("kierownik", -1);
    if (!g_reporter) exit(1);

    struct sigaction sa;
    sa.sa_sigaction = obsluga_potwierdzenia;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN + 1, &sa, NULL);

    reporter_info(g_reporter, "Kierownik PID: %d", getpid());
    g_hala->kierownik_pid = getpid();

    int czas_pracy = 0;
    int liczba_wstrzyman = 0;
    int liczba_wznowien = 0;

    while (!g_hala->mecz_zakonczony && !g_hala->ewakuacja) {
        sem_wait_ipc(sem_id, SEM_MAIN);

        for (int s = 0; s < LICZBA_SEKTOROW; s++) {
            int zapelnienie = g_hala->kibice_w_sektorze_ilosc[s];

            // wstrzymaj jesli pelny
            if (zapelnienie >= POJEMNOSC_SEKTORA) {
                if (!g_hala->wejscia[s].wstrzymane) {
                    sem_post_ipc(sem_id, SEM_MAIN);
                    wstrzymaj_sektor(s);
                    liczba_wstrzyman++;
                    sem_wait_ipc(sem_id, SEM_MAIN);
                }
            }

            // wznow jesli sa miejsca
            if (zapelnienie < POJEMNOSC_SEKTORA * 0.9) {
                if (g_hala->wejscia[s].wstrzymane) {
                    sem_post_ipc(sem_id, SEM_MAIN);
                    wznow_sektor(s);
                    liczba_wznowien++;
                    sem_wait_ipc(sem_id, SEM_MAIN);
                }
            }
        }

        // losowy incydent
        if (czas_pracy > 30 && (rand() % 1000) == 0) {
            reporter_critical(g_reporter, "Zagrozenie wykryte");
            sem_post_ipc(sem_id, SEM_MAIN);
            ewakuuj_cala_hale();
            break;
        }

        sem_post_ipc(sem_id, SEM_MAIN);
        sleep(1);
        czas_pracy++;
    }

    if (g_hala->mecz_zakonczony && !g_hala->ewakuacja) {
        reporter_info(g_reporter, "Koniec meczu");
        ewakuuj_cala_hale();

        reporter_info(g_reporter, "Czekam na oproznienie");
        int timeout = 60;
        while (!czy_wszystkie_sektory_oprozniaje() && timeout > 0) {
            usleep(500000);
            timeout--;
        }

        if (czy_wszystkie_sektory_oprozniaje()) {
            reporter_info(g_reporter, "Sektory oproznioje");
        } else {
            reporter_warning(g_reporter, "Timeout");
        }
    }

    reporter_info(g_reporter, "Statystyki: %ds, %d wstrzyman, %d wznowien",
                 czas_pracy, liczba_wstrzyman, liczba_wznowien);

    reporter_close(g_reporter);
    shmdt(g_hala);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uzycie: %s <shm_id> <sem_id> <msg_id>\n", argv[0]);
        return 1;
    }

    proces_kierownika(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    return 0;
}