// kasjer.cpp - Wersja z "downgrade" biletów (2->1) przy braku miejsc
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>
#include "../include/common.h"

typedef struct {
    FILE *file;
    int id;
} Logger;

void log_msg(Logger *logger, const char *level, const char *msg) {
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(logger->file, "[%s] [Kasa %d] [%s] %s\n", timestr, logger->id, level, msg);
    fflush(logger->file);
    printf("[Kasa %d] %s\n", logger->id, msg);
}

bool obsluz_vip(int id, Hala *hala, Logger *logger) {
    if (hala->rozmiar_kolejki_kasy_vip <= 0) return false;

    int id_vip = hala->kolejka_do_kasy_VIP[0];
    Kibic* vip = &hala->kibice_vip[id_vip];

    for (int j = 0; j < hala->rozmiar_kolejki_kasy_vip - 1; j++) {
        hala->kolejka_do_kasy_VIP[j] = hala->kolejka_do_kasy_VIP[j + 1];
    }
    hala->rozmiar_kolejki_kasy_vip--;

    long target_mtype = id_vip + VIP_MTYPE_OFFSET;

    if (hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] >= POJEMNOSC_VIP) {
        log_msg(logger, "WARNING", "Brak miejsc w VIP");
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

    char msg[128]; snprintf(msg, sizeof(msg), "VIP %d kupił bilet", id_vip);
    log_msg(logger, "INFO", msg);

    struct moj_komunikat kom;
    kom.mtype = target_mtype;
    kom.kibic_id = id_vip;
    kom.akcja = 1;
    kom.sektor = SEKTOR_VIP;
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
    if (id_towarzysza >= K_KIBICOW + K_KIBICOW) return -1;

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
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) exit(1);

    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "kasjer_%d.log", id);
    Logger logger = {fopen(log_filename, "w"), id};
    log_msg(&logger, "INFO", "Kasa otwarta");

    while (1) {
        sem_wait_ipc(sem_id, SEM_MAIN);

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
                sem_post_ipc(sem_id, SEM_MAIN);
                continue;
            }
            log_msg(&logger, "INFO", "Bilety wyprzedane - koniec");
            hala->otwarte_kasy--;
            sem_post_ipc(sem_id, SEM_MAIN);
            fclose(logger.file);
            shmdt(hala);
            exit(0);
        }

        if (obsluz_vip(id, hala, &logger)) {
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(300000);
            continue;
        }

        if (hala->otwarte_kasy > 2 &&
            hala->rozmiar_kolejki_kasy < (K_KIBICOW / LICZBA_KAS) * (hala->otwarte_kasy - 1) &&
            id == hala->otwarte_kasy) {
            hala->otwarte_kasy--;
            log_msg(&logger, "INFO", "Zamykam kasę (mały ruch)");
            sem_post_ipc(sem_id, SEM_MAIN);
            fclose(logger.file);
            shmdt(hala);
            exit(0);
        }

        if (hala->rozmiar_kolejki_kasy > 0) {
            int id_kibica = hala->kolejka_do_kasy[0];
            Kibic* kibic = &hala->kibice[id_kibica];

            for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++)
                hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
            hala->rozmiar_kolejki_kasy--;

            int liczba_biletow = 1;
            if (kibic->jest_dzieckiem) liczba_biletow = 2;
            else liczba_biletow = 1 + (rand() % 2);

            int sektor = znajdz_wolny_sektor(hala, liczba_biletow);

            // ================================================================
            // FIX: Jeśli brak miejsc na 2 bilety, spróbuj sprzedać 1 (jeśli dorosły)
            // ================================================================
            if (sektor == -1 && liczba_biletow == 2 && !kibic->jest_dzieckiem) {
                char warn[128];
                snprintf(warn, sizeof(warn), "Kibic %d chciał 2 bilety, ale brak par miejsc. Próba sprzedaży 1.", id_kibica);
                log_msg(&logger, "WARNING", warn);

                liczba_biletow = 1;
                sektor = znajdz_wolny_sektor(hala, liczba_biletow);
            }

            long target_mtype = id_kibica + 1;

            if (sektor == -1) {
                log_msg(&logger, "WARNING", "Brak miejsc dla kibica w jednym sektorze");
                struct moj_komunikat kom;
                kom.mtype = target_mtype;
                kom.akcja = 0;
                msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), 0);
            } else {
                hala->sprzedane_bilety_w_sektorze[sektor] += liczba_biletow;
                hala->sprzedane_bilety += liczba_biletow;
                kibic->ma_bilet = 1;
                kibic->sektor = sektor;
                kibic->liczba_biletow = liczba_biletow;

                if (liczba_biletow == 2) {
                    int id_drugiego = utworz_towarzysza(hala, id_kibica, sektor);
                    char msg[128];
                    if (kibic->jest_dzieckiem)
                        snprintf(msg, sizeof(msg), "Dziecko %d kupiło 2 bilety -> Opiekun %d", id_kibica, id_drugiego);
                    else
                        snprintf(msg, sizeof(msg), "Kibic %d kupił 2 bilety -> Towarzysz %d", id_kibica, id_drugiego);
                    log_msg(&logger, "INFO", msg);
                }
                else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Kibic %d kupił 1 bilet", id_kibica);
                    log_msg(&logger, "INFO", msg);
                }

                char msg[256];
                snprintf(msg, sizeof(msg), "Sprzedano: %d bilet(ów) do sektora %d (Suma: %d/%d)",
                        liczba_biletow, sektor, hala->sprzedane_bilety, LIMIT_SPRZEDAZY);
                log_msg(&logger, "INFO", msg);

                struct moj_komunikat kom;
                kom.mtype = target_mtype;
                kom.kibic_id = id_kibica;
                kom.akcja = 1;
                kom.sektor = sektor;
                msgsnd(msg_id, &kom, sizeof(kom) - sizeof(long), 0);
            }
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(300000);
        } else {
            sem_post_ipc(sem_id, SEM_MAIN);
            usleep(100000);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    proces_kasy(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}