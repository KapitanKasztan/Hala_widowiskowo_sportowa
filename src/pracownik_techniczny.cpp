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

static volatile sig_atomic_t wstrzymane = 0;
static volatile sig_atomic_t ewakuacja = 0;
static int g_sektor_id = -1;
static int g_stanowisko_id = -1;
static Logger *g_reporter = NULL;

void obsluga_wstrzymaj(int sig) {
    (void)sig;
    wstrzymane = 1;
    if (g_reporter) {
        reporter_warning(g_reporter, "Otrzymano sygnal WSTRZYMAJ");
    }
}

void obsluga_wznow(int sig) {
    (void)sig;
    wstrzymane = 0;
    if (g_reporter) {
        reporter_info(g_reporter, "Otrzymano sygnal WZNOW");
    }
}

void obsluga_ewakuacja(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    ewakuacja = 1;
    if (g_reporter) {
        reporter_critical(g_reporter, "Otrzymano sygnal EWAKUACJA");
    }
}

void proces_stanowiska(int sektor_id, int stanowisko_id, int shm_id, int sem_id, int msg_id) {
    g_sektor_id = sektor_id;
    g_stanowisko_id = stanowisko_id;

    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) exit(1);

    char process_name[64];
    snprintf(process_name, sizeof(process_name), "stanowisko_%d_%d", sektor_id, stanowisko_id);
    g_reporter = reporter_init(process_name, -1);
    if (!g_reporter) exit(1);

    signal(SIGUSR1, obsluga_wstrzymaj);
    signal(SIGUSR2, obsluga_wznow);
    struct sigaction sa;
    sa.sa_sigaction = obsluga_ewakuacja;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    reporter_info(g_reporter, "Stanowisko uruchomione - sektor %d, stanowisko %d",
                 sektor_id, stanowisko_id);

    WejscieDoSektora *wejscie = &hala->wejscia[sektor_id];
    int aktualnie_kontrolowani[MAX_OSOB_NA_STANOWISKU];
    int liczba_na_stanowisku = 0;
    int aktualna_druzyna = -1;

    int wpuszczonych_laczni = 0;
    int przepuszczen_grup = 0;

    while (1) {
        if (ewakuacja || hala->ewakuacja) {
            reporter_info(g_reporter, "Ewakuacja - zamykanie stanowiska (wpuszczono: %d)",
                         wpuszczonych_laczni);
            reporter_close(g_reporter);
            shmdt(hala);
            exit(0);
        }

        sem_wait_ipc(sem_id, SEM_MAIN);
        int koniec = (hala->kibice_na_hali >= LIMIT_SPRZEDAZY) || hala->mecz_zakonczony;
        sem_post_ipc(sem_id, SEM_MAIN);

        if (koniec) {
            reporter_info(g_reporter, "Koniec meczu - zamykanie stanowiska");
            reporter_info(g_reporter, "Statystyki: wpuszczono %d osob, przepuszczono %d grup",
                         wpuszczonych_laczni, przepuszczen_grup);
            reporter_close(g_reporter);
            shmdt(hala);
            exit(0);
        }

        // wpuszczamy kibica na hale
        if (liczba_na_stanowisku > 0) {
            int kibic_id = aktualnie_kontrolowani[0];
            Kibic *kibic = &hala->kibice[kibic_id];

            sem_wait_ipc(sem_id, SEM_MAIN);
            hala->kibice_w_sektorze[sektor_id][hala->kibice_w_sektorze_ilosc[sektor_id]++] = kibic_id;
            hala->kibice_na_hali++;
            kibic->na_hali = 1;
            sem_post_ipc(sem_id, SEM_MAIN);

            struct moj_komunikat kom;
            kom.mtype = kibic_id + 1;
            kom.kibic_id = kibic_id;
            kom.akcja = MSG_KONTROLA;
            msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), 0);

            reporter_info(g_reporter, "Wpuszczono kibica %d do sektora %d (druzyna %d)",
                         kibic_id, sektor_id, kibic->druzyna);
            wpuszczonych_laczni++;

            for (int i = 0; i < liczba_na_stanowisku - 1; i++)
                aktualnie_kontrolowani[i] = aktualnie_kontrolowani[i + 1];
            liczba_na_stanowisku--;

            if (liczba_na_stanowisku == 0) aktualna_druzyna = -1;
            usleep(200000);
        }

        sem_wait_ipc(sem_id, SEM_WEJSCIA + sektor_id);
        int czy_wstrzymane = wejscie->wstrzymane || wstrzymane;
        sem_post_ipc(sem_id, SEM_WEJSCIA + sektor_id);

        if (czy_wstrzymane) {
            usleep(200000);
            continue;
        }

        // priorytet dla agresywnych
        sem_wait_ipc(sem_id, SEM_WEJSCIA + sektor_id);
        if (wejscie->rozmiar_kolejki > 0 && liczba_na_stanowisku == 0) {
             int znaleziono = 0;
             for (int i = 0; i < wejscie->rozmiar_kolejki && !znaleziono; i++) {
                 int id = wejscie->kolejka_do_kontroli[i];
                 if (hala->kibice[id].przepuscil >= MAX_PRZEPUSZCZONYCH) {
                     aktualnie_kontrolowani[liczba_na_stanowisku++] = id;
                     aktualna_druzyna = hala->kibice[id].druzyna;
                     hala->kibice[id].przepuscil = 0;

                     reporter_debug(g_reporter, "Priorytet dla kibica %d (przepuscil %d razy)",
                                   id, MAX_PRZEPUSZCZONYCH);

                     for(int j=i; j<wejscie->rozmiar_kolejki-1; j++)
                        wejscie->kolejka_do_kontroli[j] = wejscie->kolejka_do_kontroli[j+1];
                     wejscie->rozmiar_kolejki--;
                     znaleziono = 1;
                 }
             }
        }

        // dodajemy ludzi do stanowiska
        while (wejscie->rozmiar_kolejki > 0 && liczba_na_stanowisku < MAX_OSOB_NA_STANOWISKU) {
            int kibic_id = wejscie->kolejka_do_kontroli[0];
            Kibic *kibic = &hala->kibice[kibic_id];

            if (liczba_na_stanowisku == 0) {
                aktualnie_kontrolowani[liczba_na_stanowisku++] = kibic_id;
                aktualna_druzyna = kibic->druzyna;

                for(int j=0; j<wejscie->rozmiar_kolejki-1; j++)
                    wejscie->kolejka_do_kontroli[j] = wejscie->kolejka_do_kontroli[j+1];
                wejscie->rozmiar_kolejki--;
            } else if (kibic->druzyna == aktualna_druzyna) {
                aktualnie_kontrolowani[liczba_na_stanowisku++] = kibic_id;

                for(int j=0; j<wejscie->rozmiar_kolejki-1; j++)
                    wejscie->kolejka_do_kontroli[j] = wejscie->kolejka_do_kontroli[j+1];
                wejscie->rozmiar_kolejki--;
            } else {
                // inna druzyna - przepuszczamy
                if (kibic->przepuscil < MAX_PRZEPUSZCZONYCH) {
                    kibic->przepuscil++;
                    przepuszczen_grup++;

                    int tmp = wejscie->kolejka_do_kontroli[0];
                    for(int j=0; j<wejscie->rozmiar_kolejki-1; j++)
                        wejscie->kolejka_do_kontroli[j] = wejscie->kolejka_do_kontroli[j+1];
                    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki-1] = tmp;
                }
                break;
            }
        }
        sem_post_ipc(sem_id, SEM_WEJSCIA + sektor_id);
        usleep(100000);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 6) return 1;
    proces_stanowiska(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    return 0;
}