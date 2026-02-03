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
#include "include/logger.h"

volatile sig_atomic_t zakoncz = 0;
int g_shm_id = -1, g_sem_id = -1, g_msg_id = -1;
Logger *g_main_reporter = NULL;
Logger *g_summary_reporter = NULL;

void obsluga_sygnalu(int sig) {
    if (sig == SIGINT) {
        reporter_warning(g_main_reporter, "Otrzymano sygnal SIGINT - konczenie symulacji");
        zakoncz = 1;
    }
}

void usun_zasoby() {
    reporter_info(g_main_reporter, "Usuwanie IPC");
    if (g_msg_id >= 0) msgctl(g_msg_id, IPC_RMID, NULL);
    if (g_sem_id >= 0) semctl(g_sem_id, 0, IPC_RMID);
    if (g_shm_id >= 0) shmctl(g_shm_id, IPC_RMID, NULL);
}

void wyswietl_status(Hala *hala) {
    sem_wait_ipc(hala->sem_id, SEM_MAIN);

    printf("\n=== STATUS ===\n");
    printf("Sprzedane: %d/%d | Na hali: %d\n",
           hala->sprzedane_bilety, LIMIT_SPRZEDAZY, hala->kibice_na_hali);
    printf("Otwarte kasy: %d | Kolejka: %d\n",
           hala->otwarte_kasy, hala->rozmiar_kolejki_kasy);
    printf("Sektory: ");
    for (int i = 0; i < LICZBA_SEKTOROW; i++)
        printf("[%d:%d] ", i, hala->sprzedane_bilety_w_sektorze[i]);
    printf("[VIP:%d]\n", hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]);
    printf("===============\n");

    reporter_info(g_main_reporter,
                  "STATUS - Sprzedane: %d/%d, Na hali: %d, Kasy: %d, Kolejka: %d",
                  hala->sprzedane_bilety, LIMIT_SPRZEDAZY, hala->kibice_na_hali,
                  hala->otwarte_kasy, hala->rozmiar_kolejki_kasy);

    sem_post_ipc(hala->sem_id, SEM_MAIN);
}

void aktualizuj_kasy(Hala *hala) {
    sem_wait_ipc(g_sem_id, SEM_MAIN);
    int kolejka = hala->rozmiar_kolejki_kasy;
    int aktywne = hala->otwarte_kasy;
    int sprzedane = hala->sprzedane_bilety;
    sem_post_ipc(g_sem_id, SEM_MAIN);

    if (sprzedane >= LIMIT_SPRZEDAZY) return;

    int prog = K_KIBICOW / 10;
    int potrzebne = 1 + (kolejka / prog);
    if (potrzebne < 2) potrzebne = 2;
    if (potrzebne > LICZBA_KAS) potrzebne = LICZBA_KAS;

    // otwieranie
    while (aktywne < potrzebne) {
        int nowe_id = aktywne + 1;
        pid_t pid = fork();
        if (pid == 0) {
            char id[16], s1[16], s2[16], s3[16];
            sprintf(id, "%d", nowe_id);
            sprintf(s1, "%d", g_shm_id);
            sprintf(s2, "%d", g_sem_id);
            sprintf(s3, "%d", g_msg_id);
            execl("./kasjer", "kasjer", id, s1, s2, s3, NULL);
            exit(1);
        }
        sem_wait_ipc(g_sem_id, SEM_MAIN);
        hala->kasy_pids[aktywne] = pid;
        hala->otwarte_kasy++;
        sem_post_ipc(g_sem_id, SEM_MAIN);

        printf("[MAIN] Otwieram kase %d (kolejka: %d)\n", nowe_id, kolejka);
        reporter_info(g_main_reporter, "Otwarto kase %d", nowe_id);

        aktywne++;
    }

    // zamykanie
    if (aktywne > 2) {
        if (kolejka < prog * (aktywne - 1)) {
            int idx = aktywne - 1;
            pid_t pid = hala->kasy_pids[idx];
            if (pid > 0) {
                kill(pid, SIGTERM);
                waitpid(pid, NULL, WNOHANG);
            }
            sem_wait_ipc(g_sem_id, SEM_MAIN);
            hala->otwarte_kasy--;
            hala->kasy_pids[idx] = 0;
            sem_post_ipc(g_sem_id, SEM_MAIN);

            printf("[MAIN] Zamykam kase %d\n", idx + 1);
            reporter_info(g_main_reporter, "Zamknieto kase %d", idx + 1);
        }
    }
}

void uruchom_kierownika(int shm_id, int sem_id, int msg_id) {
    if (fork() == 0) {
        char s1[16], s2[16], s3[16];
        sprintf(s1, "%d", shm_id);
        sprintf(s2, "%d", sem_id);
        sprintf(s3, "%d", msg_id);
        execl("./kierownik", "kierownik", s1, s2, s3, NULL);
        exit(1);
    }
    reporter_info(g_main_reporter, "Uruchomiono kierownika");
}

void uruchom_sektory(int shm_id, int sem_id, int msg_id) {
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            if (fork() == 0) {
                char sek[16], sta[16], s1[16], s2[16], s3[16];
                sprintf(sek, "%d", s);
                sprintf(sta, "%d", st);
                sprintf(s1, "%d", shm_id);
                sprintf(s2, "%d", sem_id);
                sprintf(s3, "%d", msg_id);
                execl("./pracownik_techniczny", "pracownik_techniczny",
                      sek, sta, s1, s2, s3, NULL);
                exit(1);
            }
        }
    }
    reporter_info(g_main_reporter, "Uruchomiono pracownikow");
}

void uruchom_kasy_startowe(Hala *hala, int shm_id, int sem_id, int msg_id) {
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            char id[16], s1[16], s2[16], s3[16];
            sprintf(id, "%d", i + 1);
            sprintf(s1, "%d", shm_id);
            sprintf(s2, "%d", sem_id);
            sprintf(s3, "%d", msg_id);
            execl("./kasjer", "kasjer", id, s1, s2, s3, NULL);
            exit(1);
        }
        hala->kasy_pids[i] = pid;
        hala->otwarte_kasy++;
    }
    reporter_info(g_main_reporter, "Otwarto 2 kasy");
}

void proces_generatora(int shm_id, int sem_id, int msg_id) {
    srand(getpid());

    for (int i = 0; i < K_KIBICOW; i++) {
        usleep(rand() % 150000);
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i);
            sprintf(s1, "%d", shm_id);
            sprintf(s2, "%d", sem_id);
            sprintf(s3, "%d", msg_id);
            execl("./kibic", "kibic", idx, s1, s2, s3, NULL);
            exit(1);
        }
    }

    for (int i = 0; i < POJEMNOSC_VIP; i++) {
        usleep(rand() % 300000);
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i);
            sprintf(s1, "%d", shm_id);
            sprintf(s2, "%d", sem_id);
            sprintf(s3, "%d", msg_id);
            execl("./kibic_vip", "kibic_vip", idx, s1, s2, s3, NULL);
            exit(1);
        }
    }
    exit(0);
}

bool init_ipc(Hala *&hala) {
    key_t k1=ftok(".",'S'), k2=ftok(".",'M'), k3=ftok(".",'Q');
    g_shm_id = shmget(k1, sizeof(Hala), IPC_CREAT|0666);
    g_sem_id = semget(k2, LICZBA_SEMAFOROW, IPC_CREAT|0666);
    g_msg_id = msgget(k3, IPC_CREAT|0666);

    if(g_shm_id<0||g_sem_id<0||g_msg_id<0) {
        reporter_error(g_main_reporter, "Blad IPC");
        return false;
    }

    hala = (Hala*)shmat(g_shm_id,NULL,0);
    sem_init_ipc(g_sem_id, SEM_MAIN, 1);
    sem_init_ipc(g_sem_id, SEM_KASY, 1);
    sem_init_ipc(g_sem_id, SEM_KOLEJKA, 1);
    for(int i=0;i<LICZBA_SEKTOROW;i++) sem_init_ipc(g_sem_id, SEM_WEJSCIA+i, 1);

    memset(hala,0,sizeof(Hala));
    hala->shm_key=k1; hala->sem_key=k2; hala->msg_key=k3;
    hala->shm_id=g_shm_id; hala->sem_id=g_sem_id; hala->msg_id=g_msg_id;

    reporter_info(g_main_reporter, "IPC OK");
    return true;
}

void inicjalizuj_kibicow(Hala *hala) {
    int dzieci = 0;

    for (int i = 0; i < K_KIBICOW; i++) {
        hala->kibice[i].id = i;
        hala->kibice[i].druzyna = i % 2;
        hala->kibice[i].sektor = -1;
        hala->kibice[i].jest_dzieckiem = (rand() % 100 < 10) ? 1 : 0;
        if (hala->kibice[i].jest_dzieckiem) dzieci++;
    }
    hala->liczba_kibicow = K_KIBICOW;

    for (int i = 0; i < POJEMNOSC_VIP; i++) {
        hala->kibice_vip[i].id = i;
        hala->kibice_vip[i].sektor = SEKTOR_VIP;
    }

    reporter_info(g_main_reporter, "Kibice: %d (dzieci: %d), VIP: %d",
                 K_KIBICOW, dzieci, POJEMNOSC_VIP);
}

void zapisz_statystyki(int nr, Hala *hala, time_t start, time_t koniec) {
    double czas = difftime(koniec, start);

    sem_wait_ipc(g_sem_id, SEM_MAIN);

    reporter_info(g_summary_reporter, "========================================");
    reporter_info(g_summary_reporter, "MECZ #%d", nr);
    reporter_info(g_summary_reporter, "========================================");
    reporter_info(g_summary_reporter, "Czas: %.0f s", czas);
    reporter_info(g_summary_reporter, "Sprzedane: %d/%d",
                 hala->sprzedane_bilety, LIMIT_SPRZEDAZY);
    reporter_info(g_summary_reporter, "Na hali: %d", hala->kibice_na_hali);

    reporter_info(g_summary_reporter, "Sektory:");
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        reporter_info(g_summary_reporter, "  [%d]: %d/%d",
                     i, hala->sprzedane_bilety_w_sektorze[i], POJEMNOSC_SEKTORA);
    }
    reporter_info(g_summary_reporter, "  [VIP]: %d/%d",
                 hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP], POJEMNOSC_VIP);
    reporter_info(g_summary_reporter, "========================================");

    sem_post_ipc(g_sem_id, SEM_MAIN);
}

int main(int argc, char *argv[]) {
    int czas_tp = 5;
    int tryb_inf = 0;

    if (argc > 1) czas_tp = atoi(argv[1]);
    if (argc > 2 && strcmp(argv[2], "--infinite") == 0) {
        tryb_inf = 1;
        printf("\n[MAIN] Tryb nieskonczonosci\n\n");
    }

    srand(time(NULL));

    g_main_reporter = reporter_init("main", -1);
    g_summary_reporter = reporter_init("summary", -1);

    if (!g_main_reporter || !g_summary_reporter) return 1;

    reporter_info(g_main_reporter, "START");

    struct sigaction sa;
    sa.sa_handler = obsluga_sygnalu;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGTERM, SIG_IGN);

    int nr = 1;

    while (1) {
        time_t start = time(NULL);

        printf("\nMECZ #%d\n", nr);
        reporter_info(g_main_reporter, "MECZ #%d", nr);

        Hala *hala;
        if (!init_ipc(hala)) break;

        inicjalizuj_kibicow(hala);
        uruchom_kierownika(g_shm_id, g_sem_id, g_msg_id);
        uruchom_sektory(g_shm_id, g_sem_id, g_msg_id);
        uruchom_kasy_startowe(hala, g_shm_id, g_sem_id, g_msg_id);

        if (fork() == 0) proces_generatora(g_shm_id, g_sem_id, g_msg_id);

        printf("\n[MAIN] Start za %d s\n", czas_tp);
        sleep(czas_tp);

        hala->mecz_rozpoczety = 1;
        reporter_info(g_main_reporter, "Mecz rozpoczety");

        while (!zakoncz) {
            wyswietl_status(hala);
            aktualizuj_kasy(hala);

            sem_wait_ipc(hala->sem_id, SEM_MAIN);
            int na_hali = hala->kibice_na_hali;
            int sprzedane = hala->sprzedane_bilety;
            sem_post_ipc(hala->sem_id, SEM_MAIN);

            if (na_hali >= sprzedane && sprzedane >= LIMIT_SPRZEDAZY) {
                printf("\n[MAIN] Koniec: %d/%d\n", na_hali, LIMIT_SPRZEDAZY);
                break;
            }

            sleep(2);
        }

        time_t koniec = time(NULL);

        printf("\n[MAIN] Koniec meczu\n");

        // Zabij kierownika
        if (hala->kierownik_pid > 0) {
            kill(hala->kierownik_pid, SIGKILL);
        }

        // Zabij kasy
        for (int i = 0; i < LICZBA_KAS; i++) {
            if (hala->kasy_pids[i] > 0) {
                kill(hala->kasy_pids[i], SIGKILL);
            }
        }

        // Zabij pracowników sektorów
        for (int s = 0; s < LICZBA_SEKTOROW; s++) {
            for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
                pid_t pid = hala->wejscia[s].pracownik_pids[st];
                if (pid > 0) {
                    kill(pid, SIGKILL);
                }
            }
        }

        // Poczekaj na wszystkie procesy potomne
        while (wait(NULL) > 0);
        zapisz_statystyki(nr, hala, start, koniec);

        usun_zasoby();
        shmdt(hala);

        if (!tryb_inf) break;
        if (zakoncz) break;

        nr++;
        printf("\n[MAIN] Przerwa 3s\n");
        sleep(3);
    }

    printf("\n[MAIN] Koniec\n\n");

    reporter_close(g_main_reporter);
    reporter_close(g_summary_reporter);

    return 0;
}