#include "../include/common.h"
#include <signal.h>

// Flagi sygnałów
static volatile sig_atomic_t wstrzymane = 0;
static volatile sig_atomic_t ewakuacja = 0;
static int g_sektor_id = -1;
static pid_t g_kierownik_pid = 0;

void obsluga_wstrzymaj(int sig) {
    (void)sig;
    wstrzymane = 1;
    printf("[Pracownik %d] Otrzymano sygnał WSTRZYMAJ\n", g_sektor_id);
}

void obsluga_wznow(int sig) {
    (void)sig;
    wstrzymane = 0;
    printf("[Pracownik %d] Otrzymano sygnał WZNÓW\n", g_sektor_id);
}

void obsluga_ewakuacja(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    ewakuacja = 1;
    g_sektor_id = info->si_value.sival_int;
    printf("[Pracownik %d] Otrzymano sygnał EWAKUACJA\n", g_sektor_id);
}

void proces_stanowiska(int sektor_id, int stanowisko_id, Hala *hala, sem_t *sem) {
    g_sektor_id = sektor_id;

    // Resetuj handlery sygnałów z procesu głównego
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    // Ustaw obsługę sygnałów dla pracownika
    signal(SIGUSR1, obsluga_wstrzymaj);
    signal(SIGUSR2, obsluga_wznow);

    struct sigaction sa;
    sa.sa_sigaction = obsluga_ewakuacja;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    printf("[Stanowisko %d-%d] Start kontroli\n", sektor_id, stanowisko_id);

    // Zmienna lokalna do przechowywania kibica w trakcie kontroli
    int kibic_w_kontroli = -1;
    int kibic_idx_w_kontroli = -1;

    while (1) {
        // Sprawdź ewakuację
        if (ewakuacja || hala->ewakuacja) {
            printf("[Stanowisko %d-%d] Ewakuacja - kończę\n", sektor_id, stanowisko_id);
            exit(0);
        }

        sem_wait(sem);

        // Sprawdź koniec symulacji
        if (hala->kibice_na_hali >= K_KIBICOW || hala->mecz_zakonczony) {
            sem_post(sem);
            printf("[Stanowisko %d-%d] Koniec pracy\n", sektor_id, stanowisko_id);
            exit(0);
        }

        WejscieDoSektora *wejscie = &hala->wejscia[sektor_id];
        Stanowisko *stan = &wejscie->stanowiska[stanowisko_id];

        // Sprawdź czy wstrzymane przez kierownika
        if (wejscie->wstrzymane || wstrzymane) {
            sem_post(sem);
            usleep(200000);
            continue;
        }

        // Jeśli mamy kibica w kontroli, zakończ jego kontrolę
        if (kibic_w_kontroli >= 0) {
            // Oznacz kibica jako na hali
            for (int k = 0; k < hala->liczba_kibiców; k++) {
                if (hala->kibice[k].id == kibic_w_kontroli) {
                    if (!hala->kibice[k].na_hali) {  // Sprawdź czy już nie jest na hali
                        hala->kibice[k].na_hali = 1;
                        hala->kibice_w_sektorze[sektor_id]++;
                        hala->kibice_na_hali++;
                        printf("[Stanowisko %d-%d] Kibic %d przeszedł kontrolę (na hali: %d)\n",
                               sektor_id, stanowisko_id, kibic_w_kontroli, hala->kibice_na_hali);
                    }
                    break;
                }
            }

            // Usuń z stanowiska
            for (int j = 0; j < stan->liczba_osob; j++) {
                if (stan->kibice_ids[j] == kibic_w_kontroli) {
                    for (int m = j; m < stan->liczba_osob - 1; m++) {
                        stan->kibice_ids[m] = stan->kibice_ids[m + 1];
                    }
                    stan->liczba_osob--;
                    break;
                }
            }

            if (stan->liczba_osob == 0) {
                stan->druzyna_na_stanowisku = -1;
            }

            kibic_w_kontroli = -1;
            kibic_idx_w_kontroli = -1;
        }

        // Sprawdź czy stanowisko jest pełne
        if (stan->liczba_osob >= MAX_OSOB_NA_STANOWISKU) {
            sem_post(sem);
            usleep(100000);
            continue;
        }

        // Szukaj kibica do obsługi z kolejki
        int znaleziono = 0;

        for (int i = 0; i < wejscie->rozmiar_kolejki && !znaleziono; i++) {
            int kibic_idx = wejscie->kolejka_do_kontroli[i];
            if (kibic_idx < 0 || kibic_idx >= hala->liczba_kibiców) continue;

            Kibic *kibic = &hala->kibice[kibic_idx];

            // Pomiń kibiców już w kontroli lub już na hali
            if (kibic->w_kontroli || kibic->na_hali) continue;

            int moze_wejsc = 0;

            if (stan->liczba_osob == 0) {
                moze_wejsc = 1;
            } else if (stan->druzyna_na_stanowisku == kibic->druzyna) {
                moze_wejsc = 1;
            } else {
                // Inna drużyna - sprawdź czy może gdzie indziej
                int moze_gdzie_indziej = 0;
                for (int s = 0; s < STANOWISKA_NA_SEKTOR; s++) {
                    if (s == stanowisko_id) continue;
                    Stanowisko *inne = &wejscie->stanowiska[s];
                    if (inne->liczba_osob == 0 ||
                        (inne->druzyna_na_stanowisku == kibic->druzyna &&
                         inne->liczba_osob < MAX_OSOB_NA_STANOWISKU)) {
                        moze_gdzie_indziej = 1;
                        break;
                    }
                }

                if (!moze_gdzie_indziej) {
                    kibic->przepuscil++;
                    if (kibic->przepuscil >= MAX_PRZEPUSZCZONYCH) {
                        moze_wejsc = 1;
                    }
                }
            }

            if (moze_wejsc) {
                // ATOMOWO: oznacz jako w kontroli i usuń z kolejki
                kibic->w_kontroli = 1;
                kibic_w_kontroli = kibic->id;
                kibic_idx_w_kontroli = kibic_idx;

                stan->kibice_ids[stan->liczba_osob] = kibic->id;
                stan->liczba_osob++;
                if (stan->liczba_osob == 1) {
                    stan->druzyna_na_stanowisku = kibic->druzyna;
                }

                // Usuń z kolejki
                for (int j = i; j < wejscie->rozmiar_kolejki - 1; j++) {
                    wejscie->kolejka_do_kontroli[j] = wejscie->kolejka_do_kontroli[j + 1];
                }
                wejscie->rozmiar_kolejki--;

                printf("[Stanowisko %d-%d] Kontroluję kibica %d (drużyna %c)\n",
                       sektor_id, stanowisko_id, kibic->id,
                       kibic->druzyna == DRUZYNA_A ? 'A' : 'B');

                znaleziono = 1;
            }
        }

        sem_post(sem);

        if (znaleziono) {
            // Czas kontroli - semafor jest ZWOLNIONY
            usleep(500000 + rand() % 500000);
        } else {
            usleep(100000);
        }
    }
}