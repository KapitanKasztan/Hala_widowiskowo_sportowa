// main.cpp - Wersja z poprawnym zamykaniem
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

volatile sig_atomic_t zakoncz = 0;
int g_shm_id = -1, g_sem_id = -1, g_msg_id = -1;

void obsluga_sygnalu(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[MAIN] Otrzymano sygnał %d - kończę symulację\n", sig);
        zakoncz = 1;
    }
}

// Funkcja usuwająca zasoby - wywoływana TYLKO na samym końcu
void usun_zasoby() {
    printf("[MAIN] Usuwanie zasobów IPC...\n");
    if (g_msg_id >= 0) msgctl(g_msg_id, IPC_RMID, NULL);
    if (g_sem_id >= 0) semctl(g_sem_id, 0, IPC_RMID);
    if (g_shm_id >= 0) shmctl(g_shm_id, IPC_RMID, NULL);
    printf("[MAIN] Zasoby usunięte.\n");
}

void wyswietl_status(Hala *hala) {
    sem_wait_ipc(hala->sem_id, SEM_MAIN);
    printf("\n=== STATUS ===\n");
    printf("Sprzedane: %d/%d | Na hali: %d\n", hala->sprzedane_bilety, LIMIT_SPRZEDAZY, hala->kibice_na_hali);
    printf("Otwarte kasy: %d | Kolejka: %d\n", hala->otwarte_kasy, hala->rozmiar_kolejki_kasy);
    printf("Sektory: ");
    for (int i = 0; i < LICZBA_SEKTOROW; i++) printf("[%d:%d] ", i, hala->sprzedane_bilety_w_sektorze[i]);
    printf("[VIP:%d]\n", hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]);
    printf("===============\n");
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

    // OTWIERANIE
    while (aktywne < potrzebne) {
        int nowe_id = aktywne + 1;
        pid_t pid = fork();
        if (pid == 0) {
            char id[16], s1[16], s2[16], s3[16];
            sprintf(id, "%d", nowe_id); sprintf(s1, "%d", g_shm_id); sprintf(s2, "%d", g_sem_id); sprintf(s3, "%d", g_msg_id);
            execl("./kasjer", "kasjer", id, s1, s2, s3, NULL);
            exit(1);
        }
        sem_wait_ipc(g_sem_id, SEM_MAIN);
        hala->kasy_pids[aktywne] = pid;
        hala->otwarte_kasy++;
        sem_post_ipc(g_sem_id, SEM_MAIN);
        printf("[MAIN] +++ Otwieram kasę nr %d (Kolejka: %d)\n", nowe_id, kolejka);
        aktywne++;
    }

    // ZAMYKANIE
    if (aktywne > 2) {
        if (kolejka < prog * (aktywne - 1)) {
            int idx_do_zamkniecia = aktywne - 1;
            pid_t pid = hala->kasy_pids[idx_do_zamkniecia];
            if (pid > 0) {
                kill(pid, SIGTERM);
                waitpid(pid, NULL, WNOHANG);
            }
            sem_wait_ipc(g_sem_id, SEM_MAIN);
            hala->otwarte_kasy--;
            hala->kasy_pids[idx_do_zamkniecia] = 0;
            sem_post_ipc(g_sem_id, SEM_MAIN);
            printf("[MAIN] --- Zamykam kasę nr %d (Mały ruch: %d)\n", idx_do_zamkniecia + 1, kolejka);
        }
    }
}

void proces_generatora(int shm_id, int sem_id, int msg_id) {
    srand(getpid());
    for (int i = 0; i < K_KIBICOW; i++) {
        usleep(rand() % 150000);
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i); sprintf(s1, "%d", shm_id); sprintf(s2, "%d", sem_id); sprintf(s3, "%d", msg_id);
            execl("./kibic", "kibic", idx, s1, s2, s3, NULL);
            exit(1);
        }
    }
    for (int i = 0; i < POJEMNOSC_VIP; i++) {
        usleep(rand() % 300000);
        if (fork() == 0) {
            char idx[16], s1[16], s2[16], s3[16];
            sprintf(idx, "%d", i); sprintf(s1, "%d", shm_id); sprintf(s2, "%d", sem_id); sprintf(s3, "%d", msg_id);
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
    if(g_shm_id<0||g_sem_id<0||g_msg_id<0) return false;
    hala = (Hala*)shmat(g_shm_id,NULL,0);
    sem_init_ipc(g_sem_id, SEM_MAIN, 1);
    sem_init_ipc(g_sem_id, SEM_KASY, 1);
    sem_init_ipc(g_sem_id, SEM_KOLEJKA, 1);
    for(int i=0;i<LICZBA_SEKTOROW;i++) sem_init_ipc(g_sem_id, SEM_WEJSCIA+i, 1);
    memset(hala,0,sizeof(Hala));
    hala->shm_key=k1; hala->sem_key=k2; hala->msg_key=k3;
    hala->shm_id=g_shm_id; hala->sem_id=g_sem_id; hala->msg_id=g_msg_id;
    return true;
}

int main(int argc, char *argv[]) {
    int czas_tp = 5;
    if (argc > 1) czas_tp = atoi(argv[1]);

    srand(time(NULL));
    struct sigaction sa; sa.sa_handler = obsluga_sygnalu; sigemptyset(&sa.sa_mask); sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    // UWAGA: Usunąłem atexit(sprzatanie), żeby kontrolować moment usunięcia ręcznie!

    Hala *hala;
    if (!init_ipc(hala)) return 1;

    for (int i = 0; i < K_KIBICOW; i++) {
        hala->kibice[i].id = i;
        hala->kibice[i].druzyna = i % 2;
        hala->kibice[i].sektor = -1;
        hala->kibice[i].jest_dzieckiem = (rand() % 100 < 10) ? 1 : 0;
    }
    hala->liczba_kibicow = K_KIBICOW;
    for (int i = 0; i < POJEMNOSC_VIP; i++) {
        hala->kibice_vip[i].id = i;
        hala->kibice_vip[i].sektor = SEKTOR_VIP;
    }

    if (fork() == 0) {
        char s1[16],s2[16],s3[16]; sprintf(s1,"%d",g_shm_id); sprintf(s2,"%d",g_sem_id); sprintf(s3,"%d",g_msg_id);
        execl("./kierownik", "kierownik", s1, s2, s3, NULL); exit(1);
    }
    for (int s=0; s<LICZBA_SEKTOROW; s++) {
        for (int st=0; st<STANOWISKA_NA_SEKTOR; st++) {
            if (fork() == 0) {
                char sek[16],sta[16],s1[16],s2[16],s3[16];
                sprintf(sek,"%d",s); sprintf(sta,"%d",st); sprintf(s1,"%d",g_shm_id); sprintf(s2,"%d",g_sem_id); sprintf(s3,"%d",g_msg_id);
                execl("./pracownik_techniczny", "pracownik_techniczny", sek, sta, s1, s2, s3, NULL); exit(1);
            }
        }
    }

    for (int i=0; i<2; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            char id[16],s1[16],s2[16],s3[16];
            sprintf(id,"%d",i+1); sprintf(s1,"%d",g_shm_id); sprintf(s2,"%d",g_sem_id); sprintf(s3,"%d",g_msg_id);
            execl("./kasjer", "kasjer", id, s1, s2, s3, NULL); exit(1);
        }
        hala->kasy_pids[i] = pid;
        hala->otwarte_kasy++;
    }

    if (fork() == 0) proces_generatora(g_shm_id, g_sem_id, g_msg_id);

    printf("\n[MAIN] Start meczu za %d s...\n", czas_tp);
    sleep(czas_tp);
    hala->mecz_rozpoczety = 1;

    while (!zakoncz) {
        wyswietl_status(hala);
        aktualizuj_kasy(hala);

        sem_wait_ipc(hala->sem_id, SEM_MAIN);
        int na_hali = hala->kibice_na_hali;
        int sprzedane = hala->sprzedane_bilety;
        sem_post_ipc(hala->sem_id, SEM_MAIN);

        if (na_hali >= sprzedane && sprzedane >= LIMIT_SPRZEDAZY) {
            printf("[MAIN] SUKCES: %d/%d (100%%)\n", na_hali, LIMIT_SPRZEDAZY);
            break;
        }
        sleep(2);
    }

    printf("\n[MAIN] Koniec. Zabijam procesy...\n");
    kill(0, SIGTERM);

    // Czekamy aż wszystko zdechnie, żeby nie usunąć semaforów pod nogami
    while (wait(NULL) > 0);

    // DOPIERO TERAZ usuwamy IPC
    usun_zasoby();
    shmdt(hala);
    return 0;
}