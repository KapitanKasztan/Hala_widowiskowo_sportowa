#include <format>

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

void proces_stanowiska(int sektor_id, int stanowisko_id, Hala *hala) {
    string logger_filename = std::format("log_sektor_{}.log", sektor_id);;
    Logger sektor_logger(logger_filename); // Create logger instance

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

    sektor_logger.log(INFO, std::format("[Stanowisko {}-{}] Start kontroli\n", sektor_id, stanowisko_id));

    int aktualna_druzyna;
    int id_kibicow[MAX_OSOB_NA_STANOWISKU];
    int liczba_osob = 0;
    WejscieDoSektora *wejscie = &hala->wejscia[sektor_id];
    Stanowisko *stan = &wejscie->stanowiska[stanowisko_id];
    // sem_init(&wejscie->sektor_sem, 1, 0);
    while (1) {
        sem_wait(&hala->main_sem);

        // Sprawdź ewakuację
        if (ewakuacja || hala->ewakuacja) {
            printf("[Stanowisko %d-%d] Ewakuacja - kończę\n", sektor_id, stanowisko_id);
            exit(0);
        }

        // Sprawdź koniec symulacji
        if (hala->kibice_na_hali >= K_KIBICOW || hala->mecz_zakonczony) {
            sem_post(&hala->main_sem);
            printf("[Stanowisko %d-%d] Koniec pracy\n", sektor_id, stanowisko_id);
            exit(0);
        }


        // Sprawdź czy wstrzymane przez kierownika
        if (wejscie->wstrzymane || wstrzymane) {
            sem_post(&hala->main_sem);
            usleep(200000);
            continue;
        }


        if (liczba_osob > 0) {
            hala->kibice_w_sektorze[sektor_id][hala->kibice_w_sektorze_ilosc[sektor_id]++] = id_kibicow[0];
            hala->kibice_na_hali++;
            Kibic *kibic = &hala->kibice[id_kibicow[0]];
            kibic->na_hali = 1;
            sem_post(&kibic->na_hali_sem);
            kibic->na_hali = 1;

            sektor_logger.log(INFO, std::format("[Kibic {}] Wpuszczony do sektora {} ze stanowiska {}", kibic->id, sektor_id, stanowisko_id));
            for (int i = 0; i < liczba_osob - 1; i++) {
                id_kibicow[i] = id_kibicow[i + 1];
            }
            liczba_osob--;
            id_kibicow[-(MAX_OSOB_NA_STANOWISKU-liczba_osob)] = -1;
        }
        //sektor_logger.log(INFO, std::format("[Stanowisko {}-{}] Na stanowisku [{} {} {}]", sektor_id, stanowisko_id, id_kibicow[0], id_kibicow[1], id_kibicow[2]));

        if (wejscie->rozmiar_kolejki > 0) {
            for (int i = 0; i < wejscie->rozmiar_kolejki && liczba_osob < MAX_OSOB_NA_STANOWISKU; i++) {
                int kibic_w_kolejce_id = wejscie->kolejka_do_kontroli[i];

                Kibic *kibic = &hala->kibice[kibic_w_kolejce_id];
                if (kibic->przepuscil >= MAX_PRZEPUSZCZONYCH) {
                    continue;
                }
                if (liczba_osob == 0) {
                    aktualna_druzyna = kibic->druzyna;
                    id_kibicow[liczba_osob++] = kibic->id;
                } else if (kibic->druzyna == aktualna_druzyna) {
                    id_kibicow[liczba_osob++] = kibic->id;
                }
            }
        }

        sem_post(&hala->main_sem);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <sektor_id> <stanowisko_id> <shm_id>\n", argv[0]);
        return 1;
    }
    int sektor_id = atoi(argv[1]);
    int stanowisko_id = atoi(argv[2]);
    int shm_id = atoi(argv[3]);
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) {
        perror("shmat");
        return 1;
    }
    proces_stanowiska(sektor_id, stanowisko_id, hala);
    return 0;
}