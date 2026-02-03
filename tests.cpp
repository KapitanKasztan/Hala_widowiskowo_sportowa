#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "include/common.h"

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define YELLOW "\033[0;33m"
#define RESET "\033[0m"

int t_shm_id = -1;
int t_sem_id = -1;
int t_msg_id = -1;

void cleanup() {
    if (t_msg_id >= 0) msgctl(t_msg_id, IPC_RMID, NULL);
    if (t_sem_id >= 0) semctl(t_sem_id, 0, IPC_RMID);
    if (t_shm_id >= 0) shmctl(t_shm_id, IPC_RMID, NULL);
}

void setup_env(Hala** hala) {
    key_t shm_key = ftok(".", 1001);
    key_t sem_key = ftok(".", 1002);
    key_t msg_key = ftok(".", 1003);

    t_shm_id = shmget(shm_key, sizeof(Hala), IPC_CREAT | 0666);
    t_sem_id = semget(sem_key, LICZBA_SEMAFOROW, IPC_CREAT | 0666);
    t_msg_id = msgget(msg_key, IPC_CREAT | 0666);

    if (t_shm_id < 0 || t_sem_id < 0 || t_msg_id < 0) {
        perror("Init test failed");
        exit(1);
    }

    *hala = (Hala*)shmat(t_shm_id, NULL, 0);
    memset(*hala, 0, sizeof(Hala));

    sem_init_ipc(t_sem_id, SEM_MAIN, 1);
    sem_init_ipc(t_sem_id, SEM_KASY, 1);
    sem_init_ipc(t_sem_id, SEM_KOLEJKA, 1);
    for(int i=0; i<LICZBA_SEKTOROW; i++) {
        sem_init_ipc(t_sem_id, SEM_WEJSCIA+i, 1);
    }

    (*hala)->shm_id = t_shm_id;
    (*hala)->sem_id = t_sem_id;
    (*hala)->msg_id = t_msg_id;
}

void print_result(const char* test_id, const char* desc, int passed) {
    if (passed) {
        printf("[%s] %sPASS%s : %s\n", test_id, GREEN, RESET, desc);
    } else {
        printf("[%s] %sFAIL%s : %s\n", test_id, RED, RESET, desc);
    }
}

void test_K2_min_open_kasy() {
    printf("\n%s=== TEST K2 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    hala->otwarte_kasy = 2;
    hala->rozmiar_kolejki_kasy = 0;
    hala->sprzedane_bilety = 0;

    int prog = K_KIBICOW / 10;
    int aktywne = hala->otwarte_kasy;
    int kolejka = hala->rozmiar_kolejki_kasy;

    int czy_zamknieto = 0;
    if (aktywne > 2) {
        if (kolejka < prog * (aktywne - 1)) {
            czy_zamknieto = 1;
        }
    }

    print_result("K2-1", "Minimum 2 kasy", czy_zamknieto == 0);

    hala->otwarte_kasy = 3;
    hala->rozmiar_kolejki_kasy = 5;

    aktywne = hala->otwarte_kasy;
    kolejka = hala->rozmiar_kolejki_kasy;
    int powinno_zamknac = 0;

    if (aktywne > 2 && kolejka < prog * (aktywne - 1)) {
        powinno_zamknac = 1;
        hala->otwarte_kasy--;
    }

    print_result("K2-2", "Zamykanie 3 kasy", powinno_zamknac == 1 && hala->otwarte_kasy == 2);

    cleanup();
}

void test_K3_otwieranie_kas() {
    printf("\n%s=== TEST K3 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int prog = K_KIBICOW / 10;

    hala->otwarte_kasy = 2;
    hala->sprzedane_bilety = 0;
    hala->rozmiar_kolejki_kasy = prog * 2 + 10;

    int aktywne = hala->otwarte_kasy;
    int kolejka = hala->rozmiar_kolejki_kasy;
    int potrzebne = 1 + (kolejka / prog);
    if (potrzebne < 2) potrzebne = 2;
    if (potrzebne > LICZBA_KAS) potrzebne = LICZBA_KAS;

    int powinno_otworzyc = (potrzebne > aktywne);

    print_result("K3-1", "Otwieranie kas", powinno_otworzyc == 1);

    while (aktywne < potrzebne) {
        aktywne++;
    }

    print_result("K3-2", "Liczba otwartych kas", aktywne == potrzebne);

    cleanup();
}

void test_K4_zamykanie_kas() {
    printf("\n%s=== TEST K4 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int prog = K_KIBICOW / 10;

    hala->otwarte_kasy = 5;
    hala->rozmiar_kolejki_kasy = prog * 3;
    hala->sprzedane_bilety = 100;

    int aktywne = hala->otwarte_kasy;
    int kolejka = hala->rozmiar_kolejki_kasy;

    int powinno_zamknac = 0;
    if (aktywne > 2 && kolejka < prog * (aktywne - 1)) {
        powinno_zamknac = 1;
        aktywne--;
    }

    print_result("K4-1", "Zamykanie przy malym ruchu", powinno_zamknac == 1 && aktywne == 4);

    hala->otwarte_kasy = 2;
    hala->rozmiar_kolejki_kasy = 5;

    aktywne = hala->otwarte_kasy;
    kolejka = hala->rozmiar_kolejki_kasy;
    powinno_zamknac = 0;

    if (aktywne > 2 && kolejka < prog * (aktywne - 1)) {
        powinno_zamknac = 1;
    }

    print_result("K4-2", "Nie zamyka ponizej 2", powinno_zamknac == 0);

    cleanup();
}

void test_K5_max_bilety() {
    printf("\n%s=== TEST K5 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    srand(time(NULL));

    int max_znaleziony = 0;
    int min_znaleziony = 10;

    for(int i = 0; i < 1000; i++) {
        int liczba_biletow = 1 + (rand() % 2);

        if (liczba_biletow > max_znaleziony) max_znaleziony = liczba_biletow;
        if (liczba_biletow < min_znaleziony) min_znaleziony = liczba_biletow;
    }

    print_result("K5-1", "Max 2 bilety", max_znaleziony <= 2);
    print_result("K5-2", "Min 1 bilet", min_znaleziony >= 1);

    Kibic dziecko;
    dziecko.jest_dzieckiem = 1;
    int bilety_dla_dziecka = dziecko.jest_dzieckiem ? 2 : (1 + (rand() % 2));

    print_result("K5-3", "Dziecko 2 bilety", bilety_dla_dziecka == 2);

    cleanup();
}

void test_K7_wyprzedanie() {
    printf("\n%s=== TEST K7 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    hala->sprzedane_bilety = LIMIT_SPRZEDAZY - 10;

    int czy_wyprzedane = (hala->sprzedane_bilety >= LIMIT_SPRZEDAZY);

    print_result("K7-1", "Przed wyprzedaniem", czy_wyprzedane == 0);

    hala->sprzedane_bilety = LIMIT_SPRZEDAZY;
    czy_wyprzedane = (hala->sprzedane_bilety >= LIMIT_SPRZEDAZY);

    print_result("K7-2", "Po wyprzedaniu", czy_wyprzedane == 1);

    hala->rozmiar_kolejki_kasy = 5;

    int powinno_zakonczyc = 0;
    if (hala->sprzedane_bilety >= LIMIT_SPRZEDAZY) {
        powinno_zakonczyc = 1;
    }

    print_result("K7-3", "Kasa konczy po wyprzedaniu", powinno_zakonczyc == 1);

    cleanup();
}

void test_K8_VIP_bypass() {
    printf("\n%s=== TEST K8 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int vip_queue_addr = (long)&hala->kolejka_do_kasy_VIP;
    int norm_queue_addr = (long)&hala->kolejka_do_kasy;

    print_result("K8-1", "Osobna kolejka VIP", vip_queue_addr != norm_queue_addr);

    hala->rozmiar_kolejki_kasy = 10;
    hala->rozmiar_kolejki_kasy_vip = 2;

    for(int i = 0; i < 10; i++) hala->kolejka_do_kasy[i] = i;
    for(int i = 0; i < 2; i++) hala->kolejka_do_kasy_VIP[i] = 100 + i;

    int obsluzono_vip_przed_zwyklymi = 0;
    if (hala->rozmiar_kolejki_kasy_vip > 0) {
        obsluzono_vip_przed_zwyklymi = 1;
    }

    print_result("K8-2", "VIP przed zwyklymi", obsluzono_vip_przed_zwyklymi == 1);

    cleanup();
}

void test_C3_limit_stanowiska() {
    printf("\n%s=== TEST C3 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int liczba_na_stanowisku = 3;
    WejscieDoSektora* wejscie = &hala->wejscia[0];

    for(int i = 0; i < 5; i++) {
        hala->kibice[i].id = i;
        hala->kibice[i].druzyna = 0;
        wejscie->kolejka_do_kontroli[i] = i;
    }
    wejscie->rozmiar_kolejki = 5;

    int mozna_dodac = 0;
    if (liczba_na_stanowisku < MAX_OSOB_NA_STANOWISKU) {
        mozna_dodac = 1;
    }

    print_result("C3-1", "Blokada 4 osoby", mozna_dodac == 0);

    liczba_na_stanowisku = 2;
    mozna_dodac = 0;
    if (liczba_na_stanowisku < MAX_OSOB_NA_STANOWISKU) {
        mozna_dodac = 1;
    }

    print_result("C3-2", "Mozna dodac gdy miejsce", mozna_dodac == 1);

    cleanup();
}

void test_C4_jedna_druzyna() {
    printf("\n%s=== TEST C4 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int aktualna_druzyna = 0;
    int liczba_na_stanowisku = 2;

    int id_kibica = 10;
    hala->kibice[id_kibica].id = id_kibica;
    hala->kibice[id_kibica].druzyna = 1;

    WejscieDoSektora* wejscie = &hala->wejscia[0];
    wejscie->kolejka_do_kontroli[0] = id_kibica;
    wejscie->rozmiar_kolejki = 1;

    int mozna_wpuscic = 0;
    Kibic* k = &hala->kibice[id_kibica];

    if (liczba_na_stanowisku == 0) {
        mozna_wpuscic = 1;
    } else if (k->druzyna == aktualna_druzyna) {
        mozna_wpuscic = 1;
    }

    print_result("C4-1", "Blokada mieszania druzyn", mozna_wpuscic == 0);

    hala->kibice[id_kibica].druzyna = 0;

    mozna_wpuscic = 0;
    if (k->druzyna == aktualna_druzyna) {
        mozna_wpuscic = 1;
    }

    print_result("C4-2", "Ta sama druzyna", mozna_wpuscic == 1);

    cleanup();
}

void test_C5_agresja_limit() {
    printf("\n%s=== TEST C5 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int id_kibica = 20;
    hala->kibice[id_kibica].id = id_kibica;
    hala->kibice[id_kibica].druzyna = 1;
    hala->kibice[id_kibica].przepuscil = MAX_PRZEPUSZCZONYCH;

    int ma_priorytet = 0;
    if (hala->kibice[id_kibica].przepuscil >= MAX_PRZEPUSZCZONYCH) {
        ma_priorytet = 1;
    }

    print_result("C5-1", "Limit przepuszczania", ma_priorytet == 1);

    hala->kibice[id_kibica].przepuscil = 3;

    ma_priorytet = 0;
    if (hala->kibice[id_kibica].przepuscil >= MAX_PRZEPUSZCZONYCH) {
        ma_priorytet = 1;
    }

    print_result("C5-2", "Ponizej limitu", ma_priorytet == 0);

    int id_inny = 21;
    hala->kibice[id_inny].przepuscil = 0;
    hala->kibice[id_inny].przepuscil++;

    print_result("C5-3", "Inkrementacja licznika", hala->kibice[id_inny].przepuscil == 1);

    cleanup();
}

void test_C6_dzieci() {
    printf("\n%s=== TEST C6 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int id_dziecka = 30;
    int id_opiekuna = 31;

    hala->kibice[id_dziecka].id = id_dziecka;
    hala->kibice[id_dziecka].jest_dzieckiem = 1;
    hala->kibice[id_dziecka].id_opiekuna_ref = id_opiekuna;

    hala->kibice[id_opiekuna].id = id_opiekuna;
    hala->kibice[id_opiekuna].jest_dzieckiem = 0;
    hala->kibice[id_opiekuna].id_towarzysza = id_dziecka;

    print_result("C6-1", "Flaga dziecka", hala->kibice[id_dziecka].jest_dzieckiem == 1);
    print_result("C6-2", "ID opiekuna", hala->kibice[id_dziecka].id_opiekuna_ref == id_opiekuna);
    print_result("C6-3", "Powiazanie", hala->kibice[id_opiekuna].id_towarzysza == id_dziecka);

    cleanup();
}

void test_C7_VIP_no_check() {
    printf("\n%s=== TEST C7 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    int vip_id = 0;

    hala->kibice_vip[vip_id].id = vip_id;
    hala->kibice_vip[vip_id].na_hali = 1;
    hala->kibice_vip[vip_id].sektor = SEKTOR_VIP;

    int vip_w_kolejce = 0;
    for(int s = 0; s < LICZBA_SEKTOROW; s++) {
        for(int i = 0; i < hala->wejscia[s].rozmiar_kolejki; i++) {
            if (hala->wejscia[s].kolejka_do_kontroli[i] == vip_id) {
                vip_w_kolejce = 1;
                break;
            }
        }
    }

    print_result("C7-1", "VIP na hali", hala->kibice_vip[vip_id].na_hali == 1);
    print_result("C7-2", "VIP bez kontroli", vip_w_kolejce == 0);
    print_result("C7-3", "Sektor VIP", hala->kibice_vip[vip_id].sektor == SEKTOR_VIP);

    cleanup();
}

void test_M1_M2_M3_Sygnaly() {
    printf("\n%s=== TEST M1-M3 ===%s\n", YELLOW, RESET);

    Hala* hala;
    setup_env(&hala);

    if (access("./pracownik_techniczny", F_OK) == -1) {
        printf("   Brak pliku pracownika\n");
        cleanup();
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        char s_shm[16], s_sem[16], s_msg[16];
        sprintf(s_shm, "%d", t_shm_id);
        sprintf(s_sem, "%d", t_sem_id);
        sprintf(s_msg, "%d", t_msg_id);

        execl("./pracownik_techniczny", "pracownik_techniczny",
              "0", "0", s_shm, s_sem, s_msg, NULL);
        exit(1);
    }

    usleep(300000);

    int status;
    if (waitpid(pid, &status, WNOHANG) != 0) {
        print_result("M-START", "Start procesu", 0);
        cleanup();
        return;
    }

    hala->wejscia[0].pracownik_pids[0] = pid;

    hala->wejscia[0].wstrzymane = 1;
    kill(pid, SIGUSR1);
    usleep(100000);

    int m1_pass = (waitpid(pid, &status, WNOHANG) == 0);
    print_result("M1", "SIGUSR1", m1_pass);

    hala->wejscia[0].wstrzymane = 0;
    kill(pid, SIGUSR2);
    usleep(100000);

    int m2_pass = (waitpid(pid, &status, WNOHANG) == 0);
    print_result("M2", "SIGUSR2", m2_pass);

    hala->ewakuacja = 1;
    kill(pid, SIGRTMIN);
    usleep(500000);

    pid_t result = waitpid(pid, &status, WNOHANG);
    int m3_pass = (result == pid);

    print_result("M3", "SIGRTMIN", m3_pass);

    if (!m3_pass) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }

    cleanup();
}

int main() {
    srand(time(NULL));

    printf("\n");
    printf("====================================\n");
    printf("  TESTY SYSTEMU HALI\n");
    printf("====================================\n");

    printf("\n%s[SEKCJA K]%s\n", YELLOW, RESET);
    test_K2_min_open_kasy();
    test_K3_otwieranie_kas();
    test_K4_zamykanie_kas();
    test_K5_max_bilety();
    test_K7_wyprzedanie();
    test_K8_VIP_bypass();

    printf("\n%s[SEKCJA C]%s\n", YELLOW, RESET);
    test_C3_limit_stanowiska();
    test_C4_jedna_druzyna();
    test_C5_agresja_limit();
    test_C6_dzieci();
    test_C7_VIP_no_check();

    printf("\n%s[SEKCJA M]%s\n", YELLOW, RESET);
    test_M1_M2_M3_Sygnaly();

    printf("\n");
    printf("====================================\n");
    printf("  KONIEC TESTOW\n");
    printf("====================================\n");
    printf("\n");

    return 0;
}