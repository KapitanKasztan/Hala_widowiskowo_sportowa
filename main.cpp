// C++
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <format>
#include <string.h>

#include "include/common.h"

// Globalne zmienne do obsługi sygnałów
int jest_procesem_potomnym = 0;
volatile sig_atomic_t zakoncz = 0;
int g_shm_id = -1;
Hala *g_hala = NULL;
sem_t *g_sem = NULL;

void obsluga_sygnalu(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[MAIN] Otrzymano sygnał %d - kończę symulację\n", sig);
        zakoncz = 1;
    }
}

void sprzatanie() {
    if (jest_procesem_potomnym) return;  // Don't cleanup in child processes

    if (g_hala != NULL) {
        shmdt(g_hala);
    }
    if (g_shm_id >= 0) {
        shmctl(g_shm_id, IPC_RMID, NULL);
    }
    if (g_sem != NULL) {
        sem_close(g_sem);
        sem_unlink("/hala_sem");
    }
    printf("[MAIN] Zasoby zwolnione\n");
}

void wyswietl_status(Hala *hala, sem_t *sem) {
    sem_wait(sem);
    printf("\n=== STATUS ===\n");
    printf("Sprzedane bilety: %d/%d\n", hala->sprzedane_bilety, K_KIBICOW);
    printf("Kibice na hali: %d\n", hala->kibice_na_hali);
    printf("Otwarte kasy: %d\n", hala->otwarte_kasy);
    printf("Kolejka do kasy: %d\n", hala->rozmiar_kolejki_kasy);
    printf("VIP w kolejce: %d\n", hala->rozmiar_kolejki_kasy_vip);
    printf("Bilety w sektorach: ");
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        printf("[%d:%d] ", i, hala->sprzedane_bilety_w_sektorze[i]);
    }
    printf("[VIP:%d]\n", hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]);
    printf("===============\n\n");
    sem_post(sem);
}

int main(int argc, char *argv[]) {
    // Walidacja argumentów
    int czas_tp = 5; // domyślny czas do startu meczu
    if (argc > 1) {
        czas_tp = atoi(argv[1]);
        if (czas_tp < 1 || czas_tp > 60) {
            fprintf(stderr, "Błąd: Czas Tp musi być w zakresie 1-60 sekund\n");
            return 1;
        }
    }

    srand(time(NULL));

    printf("===========================================\n");
    printf("=== SYMULACJA HALI WIDOWISKOWO-SPORTOWEJ ===\n");
    printf("===========================================\n");
    printf("Pojemność hali: %d kibiców\n", K_KIBICOW);
    printf("Liczba sektorów: %d + VIP\n", LICZBA_SEKTOROW);
    printf("Pojemność sektora: %d\n", POJEMNOSC_SEKTORA);
    printf("Max VIP: %d (0.3%% * K)\n", POJEMNOSC_VIP);
    printf("Liczba kas: %d\n", LICZBA_KAS);
    printf("Czas do startu meczu (Tp): %d s\n", czas_tp);
    printf("===========================================\n\n");

    // Obsługa sygnałów
    struct sigaction sa;
    sa.sa_handler = obsluga_sygnalu;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        return 1;
    }

    // Inicjalizacja pamięci współdzielonej
    g_shm_id = shmget(IPC_PRIVATE, sizeof(Hala), IPC_CREAT | 0600);
    if (g_shm_id < 0) {
        perror("shmget");
        return 1;
    }

    g_hala = (Hala*) shmat(g_shm_id, NULL, 0);
    if (g_hala == (void*)-1) {
        perror("shmat");
        shmctl(g_shm_id, IPC_RMID, NULL);
        return 1;
    }
    Hala *hala = g_hala;

    // Inicjalizacja struktury
    memset(hala, 0, sizeof(Hala));
    hala->sprzedane_bilety = 0;
    hala->otwarte_kasy = 0;
    hala->rozmiar_kolejki_kasy_vip = 0;
    hala->kibice_na_hali = 0;
    hala->czas_meczu = 0;
    hala->mecz_rozpoczety = 0;
    hala->mecz_zakonczony = 0;
    hala->ewakuacja = 0;
    hala->liczba_kibiców = 0;
    hala->liczba_kibiców_VIP = 0;
    hala->rozmiar_kolejki_kasy = 0;
    hala->rozmiar_dzieci = 0;

    // Inicjalizacja sektorów
    for (int i = 0; i < LICZBA_SEKTOROW + 1; i++) {
        hala->sprzedane_bilety_w_sektorze[i] = 0;
        hala->kibice_w_sektorze_ilosc[i] = 0;
    }

    // Inicjalizacja wejść do sektorów
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        hala->wejscia[s].rozmiar_kolejki = 0;
        hala->wejscia[s].wstrzymane = 0;
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            hala->wejscia[s].stanowiska[st].liczba_osob = 0;
            hala->wejscia[s].stanowiska[st].druzyna_na_stanowisku = -1;
            for (int k = 0; k < MAX_OSOB_NA_STANOWISKU; k++) {
                hala->wejscia[s].stanowiska[st].kibice_ids[k] = -1;
            }
        }
    }

    // Semafor
    sem_unlink("/hala_sem");
    g_sem = sem_open("/hala_sem", O_CREAT | O_EXCL, 0600, 1);
    if (g_sem == SEM_FAILED) {
        perror("sem_open");
        sprzatanie();
        return 1;
    }
    sem_t *hala_sem = g_sem;

    //atexit(sprzatanie);

    // Semafor
    sem_unlink("/kasy_sem");
    g_sem = sem_open("/kasy_sem", O_CREAT | O_EXCL, 0600, 1);
    if (g_sem == SEM_FAILED) {
        perror("sem_open");
        sprzatanie();
        return 1;
    }
    sem_t *kasy_sem = g_sem;


    printf("[MAIN] Uruchamiam generator kas...\n");
    pid_t pid_kasy = fork();
    if (pid_kasy < 0) {
        perror("fork generator_kas");
        return 1;
    }
    if (pid_kasy == 0) {
        generator_kas(hala, hala_sem);
        exit(0);
    }

    printf("[MAIN] Uruchamiam stanowiska kontroli...\n");
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        // Semafor
        std::string sem_name = std::format("/sektor_sem_{}\n", s);
        sem_unlink(sem_name.c_str());
        g_sem = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0600, 1);
        if (g_sem == SEM_FAILED) {
            perror("sem_open");
            sprzatanie();
            return 1;
        }
        sem_t *sektor_sem = g_sem;
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork stanowisko");
                continue;
            }
            if (pid == 0) {
                proces_stanowiska(s, st, hala, hala_sem);
                exit(0);
            }
        }
    }

    sleep(1); // Daj czas na uruchomienie kas i stanowisk

    printf("[MAIN] Generuję kibiców...\n");
    int wygenerowano_kibiców = 0;
    for (int i = 1; i <= K_KIBICOW && !zakoncz; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork kibic");
            continue;
        }
        if (pid == 0) {
            jest_procesem_potomnym = 1;
            srand(time(NULL) ^ getpid());
            int druzyna = rand() % 2;
            int jest_dzieckiem = 0; // 5% dzieci
            // int jest_dzieckiem = (rand() % 100) < 5; // 5% dzieci
            int id_opiekuna = jest_dzieckiem ? -1 : -1;

            sem_wait(hala_sem);
            int idx = hala->liczba_kibiców++;
            Kibic *kibic = &hala->kibice[idx];
            *kibic = Kibic(i, druzyna, -1, 0, 0, 0, jest_dzieckiem, id_opiekuna);
            sem_post(hala_sem);

            proces_kibica_z_kontrola(idx, kibic, hala, kasy_sem, hala_sem);
            exit(0);
        }
        wygenerowano_kibiców++;
        usleep(10000 + rand() % 10000); // 30-80ms między kibicami
    }

    printf("[MAIN] Generuję VIP-ów (%d)...\n", POJEMNOSC_VIP);
    for (int i = 0; i < POJEMNOSC_VIP && !zakoncz; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork VIP");
            continue;
        }
        if (pid == 0) {
            jest_procesem_potomnym = 1;
            srand(time(NULL) ^ getpid());

            sem_wait(hala_sem);
            int idx = hala->liczba_kibiców_VIP;
            hala->liczba_kibiców_VIP++;
            Kibic *kibic_vip = &hala->kibice_vip[idx];
            *kibic_vip = Kibic(1000 + i, 1);
            sem_post(hala_sem);

            proces_kibica_vip(idx, kibic_vip, hala, kasy_sem, hala_sem);
            exit(0);
        }
        usleep(100000 + rand() % 200000);
    }

    // Czas do startu meczu
    printf("\n[MAIN] Oczekiwanie na start meczu (Tp = %d s)...\n", czas_tp);
    for (int t = czas_tp; t > 0 && !zakoncz; t--) {
        printf("[MAIN] Start meczu za %d s...\n", t);
        sleep(1);
    }

    hala->mecz_rozpoczety = 1;
    hala->czas_meczu = 1;
    printf("\n========================================\n");
    printf("=== MECZ ROZPOCZĘTY! ===\n");
    printf("========================================\n\n");

    // Monitorowanie
    int timeout = 180; // Zwiększ timeout
    while (!zakoncz && timeout > 0) {
        wyswietl_status(hala, hala_sem);

        sem_wait(hala_sem);
        int na_hali = hala->kibice_na_hali;
        int sprzedane = hala->sprzedane_bilety;
        int vip_count = hala->liczba_kibiców_VIP;
        sem_post(hala_sem);

        // Sprawdź czy wszyscy z biletami są na hali
        if (na_hali >= sprzedane && sprzedane >= K_KIBICOW) {
            printf("[MAIN] Wszyscy kibice z biletami na hali!\n");
            break;
        }

        sleep(3);
        timeout -= 3;
    }

    // Podsumowanie
    printf("\n========================================\n");
    printf("=== PODSUMOWANIE SYMULACJI ===\n");
    printf("========================================\n");

    sem_wait(hala_sem);
    printf("Kibice na hali: %d\n", hala->kibice_na_hali);
    printf("Sprzedane bilety: %d/%d\n", hala->sprzedane_bilety, K_KIBICOW);
    printf("VIP-ów: %d\n", hala->liczba_kibiców_VIP);
    printf("\nBilety w sektorach:\n");
    for (int i = 0; i < LICZBA_SEKTOROW; i++) {
        printf("  Sektor %d: %d/%d\n", i, hala->sprzedane_bilety_w_sektorze[i], POJEMNOSC_SEKTORA);
    }
    printf("  Sektor VIP: %d\n", hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]);
    sem_post(hala_sem);

    printf("\n[MAIN] Kończę procesy potomne...\n");
    kill(0, SIGTERM);

    // Czekaj na zakończenie procesów
    while (wait(NULL) > 0);

    printf("\n=== SYMULACJA ZAKOŃCZONA ===\n");
    return 0;
}