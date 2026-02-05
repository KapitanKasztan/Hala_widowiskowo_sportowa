#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include "../include/common.h"
#include "../include/logger.h"

volatile sig_atomic_t keep_running = 1;

void handle_sigterm(int sig) {
    (void)sig;
    keep_running = 0;
}

bool obsluz_vip(int id, Hala *hala, Logger *reporter) {
    if (hala->rozmiar_kolejki_kasy_vip <= 0) return false;

    int id_vip = hala->kolejka_do_kasy_VIP[0];
    Kibic* vip = &hala->kibice_vip[id_vip];

    for (int j = 0; j < hala->rozmiar_kolejki_kasy_vip - 1; j++) {
        hala->kolejka_do_kasy_VIP[j] = hala->kolejka_do_kasy_VIP[j + 1];
    }
    hala->rozmiar_kolejki_kasy_vip--;

    long target_mtype = id_vip + VIP_MTYPE_OFFSET;

    // brak miejsc w VIP
    if (hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] >= POJEMNOSC_VIP) {
        reporter_warning(reporter, "Brak miejsc VIP dla %d", id_vip);
        struct moj_komunikat kom;
        kom.mtype = target_mtype;
        kom.kibic_id = id_vip;
        kom.akcja = 0;
        msgsnd(hala->msg_id, &kom, sizeof(kom) - sizeof(long), 0);
        return true;
    }

    hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP]++;
    hala->sprzedane_bilety++;
    vip->ma_bilet = 1;
    vip->sektor = SEKTOR_VIP;
    vip->liczba_biletow = 1;

    reporter_info(reporter, "Bilet VIP dla %d", id_vip);

    struct moj_komunikat kom;
    kom.mtype = target_mtype;
    kom.kibic_id = id_vip;
    kom.akcja = 1;
    strcpy(kom.text, "Bilet VIP");
    msgsnd(hala->msg_id, &kom, sizeof(kom) - sizeof(long), 0);
    return true;
}

int znajdz_wolny_sektor(Hala *hala, int liczba_biletow) {
    int sektor = rand() % LICZBA_SEKTOROW;
    int proby = 0;
    while (hala->sprzedane_bilety_w_sektorze[sektor] + liczba_biletow > POJEMNOSC_SEKTORA) {
        proby++;
        if (proby >= LICZBA_SEKTOROW) return -1;
        sektor = (sektor + 1) % LICZBA_SEKTOROW;
    }
    return sektor;
}

int utworz_towarzysza(Hala *hala, int id_glownego, int sektor) {
    int id_towarzysza = hala->liczba_kibicow;
    if (id_towarzysza >= K_KIBICOW * 3) return -1;

    Kibic *towarzysz = &hala->kibice[id_towarzysza];
    Kibic *glowny = &hala->kibice[id_glownego];

    towarzysz->id = id_towarzysza;
    towarzysz->druzyna = glowny->druzyna;
    towarzysz->sektor = sektor;
    towarzysz->jest_vip = 0;
    towarzysz->jest_dzieckiem = 0;
    towarzysz->ma_bilet = 1;
    towarzysz->na_hali = 0;
    towarzysz->liczba_biletow = 0;
    towarzysz->pid = 0;

    towarzysz->id_towarzysza = id_glownego;
    glowny->id_towarzysza = id_towarzysza;

    if (glowny->jest_dzieckiem) {
        glowny->id_opiekuna_ref = id_towarzysza;
        towarzysz->id_opiekuna_ref = id_glownego;
    }

    hala->liczba_kibicow++;
    return id_towarzysza;
}

void proces_kasy(int id, int shm_id, int sem_id, int msg_id) {
    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) exit(1);

    Logger *reporter = reporter_init("kasjer", id);
    if (!reporter) exit(1);

    reporter_info(reporter, "Kasa otwarta");

    int obsluzonych = 0;
    int vip_cnt = 0;
    int zwykli_cnt = 0;
    int odmowy = 0;

    while (keep_running) {
        if (sem_wait_ipc(sem_id, SEM_MAIN) == -1) {
            if (!keep_running) break;
            continue;
        }

        // wyprzedane
        if (hala->sprzedane_bilety >= LIMIT_SPRZEDAZY) {
            if (hala->rozmiar_kolejki_kasy > 0) {
                int id_kibica = hala->kolejka_do_kasy[0];
                for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++)
                    hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
                hala->rozmiar_kolejki_kasy--;

                struct moj_komunikat kom;
                kom.mtype = id_kibica + 1;
                kom.akcja = 0;
                msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), IPC_NOWAIT);

                odmowy++;
                sem_post_ipc(sem_id, SEM_MAIN);
                continue;
            }
            reporter_info(reporter, "Koniec biletow");
            reporter_info(reporter, "Obsluzono: %d (VIP:%d, zwykli:%d), odmowy:%d",
                         obsluzonych, vip_cnt, zwykli_cnt, odmowy);
            sem_post_ipc(sem_id, SEM_MAIN);
            break;
        }

        // VIP
        if (obsluz_vip(id, hala, reporter)) {
            vip_cnt++;
            obsluzonych++;
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(200000);
            continue;
        }

        // zwykli
        if (hala->rozmiar_kolejki_kasy > 0) {
            int id_kibica = hala->kolejka_do_kasy[0];
            Kibic* kibic = &hala->kibice[id_kibica];

            for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++)
                hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
            hala->rozmiar_kolejki_kasy--;

            int liczba = 1;
            // zakomenduj zeby usunac mozliwosc 2 biletow
            if (kibic->jest_dzieckiem)
                liczba = 2;
            else
                liczba = 1 + (rand() % 2);

            int sektor = znajdz_wolny_sektor(hala, liczba);
            if (sektor == -1 && liczba == 2 && !kibic->jest_dzieckiem) {
                liczba = 1;
                sektor = znajdz_wolny_sektor(hala, liczba);
            }

            long target_mtype = id_kibica + 1;

            if (sektor == -1) {
                reporter_warning(reporter, "Brak miejsc dla %d", id_kibica);
                odmowy++;

                struct moj_komunikat kom;
                kom.mtype = target_mtype;
                kom.akcja = 0;
                msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), 0);
            } else {
                hala->sprzedane_bilety_w_sektorze[sektor] += liczba;
                hala->sprzedane_bilety += liczba;
                kibic->ma_bilet = 1;
                kibic->sektor = sektor;
                kibic->liczba_biletow = liczba;

                if (liczba == 2) {
                    int tow = utworz_towarzysza(hala, id_kibica, sektor);
                    reporter_info(reporter, "Kibic %d + towarzysz %d, sektor %d",
                                 id_kibica, tow, sektor);
                } else {
                    reporter_info(reporter, "Kibic %d, sektor %d", id_kibica, sektor);
                }

                zwykli_cnt++;
                obsluzonych++;

                struct moj_komunikat kom;
                kom.mtype = target_mtype;
                kom.kibic_id = id_kibica;
                kom.akcja = 1;
                kom.sektor = sektor;
                msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), 0);
            }
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(200000);
        } else {
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(100000);
        }
    }

    reporter_info(reporter, "Zamykanie kasy");
    reporter_close(reporter);
    shmdt(hala);
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    proces_kasy(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}