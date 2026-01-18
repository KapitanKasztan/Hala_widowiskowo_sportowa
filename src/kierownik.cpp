// src/kierownik.cpp
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <semaphore.h>
#include "../include/common.h"

// Sygnały używane przez kierownika
#define SYGNAL_WSTRZYMAJ SIGUSR1  // sygnał1 - wstrzymaj wpuszczanie
#define SYGNAL_WZNOW     SIGUSR2  // sygnał2 - wznów wpuszczanie
#define SYGNAL_EWAKUACJA SIGRTMIN // sygnał3 - ewakuacja


static pid_t pracownicy_techniczni[LICZBA_SEKTOROW];
static Hala *g_hala = NULL;
static sem_t *g_sem = NULL;
static volatile sig_atomic_t sektor_oproznieny[LICZBA_SEKTOROW] = {0};

void obsluga_potwierdzenia(int sig, siginfo_t *info, void *context) {
    (void)context;
    if (sig == SIGRTMIN + 1) {
        // Pracownik techniczny potwierdził opróżnienie sektora
        int sektor = info->si_value.sival_int;
        if (sektor >= 0 && sektor < LICZBA_SEKTOROW) {
            sektor_oproznieny[sektor] = 1;
            printf("[KIEROWNIK] Sektor %d został opróżniony\n", sektor);
        }
    }
}

void kierownik_init(Hala *hala, sem_t *sem, pid_t *pidy_pracownikow) {
    g_hala = hala;
    g_sem = sem;

    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        pracownicy_techniczni[i] = pidy_pracownikow[i];
    }

    // Ustaw obsługę potwierdzenia od pracowników
    struct sigaction sa;
    sa.sa_sigaction = obsluga_potwierdzenia;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN + 1, &sa, NULL);

    printf("[KIEROWNIK] Zainicjalizowany, zarządzam %d sektorami\n", LICZBA_SEKTOROW);
}

void wstrzymaj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] Błędny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] Wstrzymuję wpuszczanie do sektora %d\n", sektor);

    sem_wait(g_sem);
    g_hala->wejscia[sektor].wstrzymane = 1;
    sem_post(g_sem);

    // Wyślij sygnał do pracownika technicznego
    if (pracownicy_techniczni[sektor] > 0) {
        kill(pracownicy_techniczni[sektor], SYGNAL_WSTRZYMAJ);
    }
}

void wznow_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] Błędny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] Wznawiam wpuszczanie do sektora %d\n", sektor);

    sem_wait(g_sem);
    g_hala->wejscia[sektor].wstrzymane = 0;
    sem_post(g_sem);

    // Wyślij sygnał do pracownika technicznego
    if (pracownicy_techniczni[sektor] > 0) {
        kill(pracownicy_techniczni[sektor], SYGNAL_WZNOW);
    }
}

void ewakuuj_sektor(int sektor) {
    if (sektor < 0 || sektor >= LICZBA_SEKTOROW) {
        printf("[KIEROWNIK] Błędny numer sektora: %d\n", sektor);
        return;
    }

    printf("[KIEROWNIK] Zarządzam EWAKUACJĘ sektora %d\n", sektor);

    sem_wait(g_sem);
    g_hala->wejscia[sektor].wstrzymane = 1;  // Najpierw wstrzymaj
    sem_post(g_sem);

    sektor_oproznieny[sektor] = 0;

    // Wyślij sygnał ewakuacji do pracownika technicznego
    if (pracownicy_techniczni[sektor] > 0) {
        union sigval val;
        val.sival_int = sektor;
        sigqueue(pracownicy_techniczni[sektor], SYGNAL_EWAKUACJA, val);
    }
}

void ewakuuj_cala_hale() {
    printf("[KIEROWNIK] === EWAKUACJA CAŁEJ HALI ===\n");

    sem_wait(g_sem);
    g_hala->ewakuacja = 1;
    sem_post(g_sem);

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

void proces_kierownika(Hala *hala, sem_t *sem, pid_t *pidy_pracownikow) {
    kierownik_init(hala, sem, pidy_pracownikow);

    printf("[KIEROWNIK] Rozpoczynam pracę\n");

    while (!hala->mecz_zakonczony && !hala->ewakuacja) {
        // Monitoruj sektory i podejmuj decyzje
        sem_wait(sem);

        for (int s = 0; s < LICZBA_SEKTOROW; s++) {
            // Przykład: wstrzymaj sektor jeśli jest pełny
            if (hala->kibice_w_sektorze_ilosc[s] >= POJEMNOSC_SEKTORA) {
                if (!hala->wejscia[s].wstrzymane) {
                    sem_post(sem);
                    wstrzymaj_sektor(s);
                    sem_wait(sem);
                }
            }
        }

        sem_post(sem);
        usleep(500000);  // Sprawdzaj co 0.5s
    }

    // Po meczu - ewakuacja
    if (hala->mecz_zakonczony) {
        ewakuuj_cala_hale();

        // Czekaj na opróżnienie wszystkich sektorów
        while (!czy_wszystkie_sektory_oprozniaje()) {
            usleep(100000);
        }

        printf("[KIEROWNIK] Wszystkie sektory opróżnione\n");
    }

    printf("[KIEROWNIK] Kończę pracę\n");
}