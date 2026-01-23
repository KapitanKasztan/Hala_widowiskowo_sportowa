#include <algorithm>
#include <format>

#include "../include/common.h"
#include "../include/logger.h"


bool sprawdz_czy_zamknac_kase_bilety(int id, Hala *hala, Logger &kasjer_logger) {
    if (hala->sprzedane_bilety >= K_KIBICOW) {
        kasjer_logger.log(INFO, std::format("[Kasa {}] Wszystkie bilety sprzedane - zamykam", id));
        hala->otwarte_kasy--;
        return true;
    }
    return false;
}

bool obsluz_vip(int id, Hala *hala, Logger &kasjer_logger) {
    if (hala->rozmiar_kolejki_kasy_vip <= 0) {
        return false;
    }
    int id_obslugiwanego_kibica = hala->kolejka_do_kasy_VIP[0];
    Kibic* obslugiwany_kibic = &hala->kibice_vip[id_obslugiwanego_kibica];
    hala->rozmiar_kolejki_kasy_vip--;

    int liczba_biletow = 1;

    while ((hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] + liczba_biletow > POJEMNOSC_VIP)) {
        liczba_biletow--;
    }
    if (liczba_biletow == 0) {
        kasjer_logger.log(WARNING, std::format("[Kasa {}] VIP {} - brak miejsc w sektorze VIP", id, obslugiwany_kibic->id));
        return true;
    }
    hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] += liczba_biletow;
    kasjer_logger.log(INFO, std::format("[Kasa {}] VIP - sprzedano {} bilet(ów) do sektora VIP", id, liczba_biletow));
    obslugiwany_kibic->ma_bilet = 1;
    obslugiwany_kibic->liczba_biletow = liczba_biletow;
    sem_post(&obslugiwany_kibic->bilet_sem);
    return true;
}

bool sprawdz_czy_zamknac_kase_kolejka(int id, Hala *hala, Logger &kasjer_logger) {
    if (hala->rozmiar_kolejki_kasy < (K_KIBICOW / LICZBA_KAS) * (hala->otwarte_kasy - 1)
        && hala->otwarte_kasy > 2
        && id == hala->otwarte_kasy) {
        hala->otwarte_kasy--;
        kasjer_logger.log(INFO, std::format("[Kasa {}] Zamykam (za mało kibiców, otwarte: {})", id, hala->otwarte_kasy));
        return true;
    }
    return false;
}

void przesun_kolejke(Hala *hala) {
    for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++) {
        hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
    }
    hala->rozmiar_kolejki_kasy--;
}

int znajdz_wolny_sektor(Hala *hala, int &liczba_biletow, int id_kibica, Logger &kasjer_logger) {
    int sektor = rand() % 8;
    int proby = 0;
    while (hala->sprzedane_bilety_w_sektorze[sektor] + liczba_biletow > K_KIBICOW / 8) {
        if (liczba_biletow == 0) {
            return -1;
        }
        if (proby == 8) {
            liczba_biletow--;
            kasjer_logger.log(WARNING, std::format("[Kasa] Zmniejszam liczbę biletów dla [Kibica {}] do {} z powodu braku miejsc", id_kibica, liczba_biletow));
            proby = 0;
        }
        sektor = (sektor + 1) % 8;
        proby++;
    }
    return sektor;
}

void proces_kasy(int id, Hala *hala, Logger &kasjer_logger) {
    kasjer_logger.log(INFO, std::format("[Kasa {}] Otwarcie", id));

    while (1) {
        sem_wait(&hala->main_sem);

        if (sprawdz_czy_zamknac_kase_bilety(id, hala, kasjer_logger)) {
            sem_post(&hala->main_sem);
            exit(0);
        }

        if (obsluz_vip(id, hala, kasjer_logger)) {
            sem_post(&hala->main_sem);
            usleep(500000);
            continue;
        }

        if (sprawdz_czy_zamknac_kase_kolejka(id, hala, kasjer_logger)) {
            sem_post(&hala->main_sem);
            exit(0);
        }

        if (hala->rozmiar_kolejki_kasy > 0) {
            int id_obslugiwanego_kibica = hala->kolejka_do_kasy[0];
            Kibic* obslugiwany_kibic = &hala->kibice[id_obslugiwanego_kibica];

            przesun_kolejke(hala);

            int liczba_biletow = 1;

            int sektor = znajdz_wolny_sektor(hala, liczba_biletow, obslugiwany_kibic->id, kasjer_logger);
            if (sektor == -1) {
                kasjer_logger.log(WARNING, std::format("[Kasa {}] Brak miejsc w żadnym sektorze dla kibica {}", id, obslugiwany_kibic->id));
                obslugiwany_kibic->ma_bilet = 0;
                przesun_kolejke(hala);
                sem_post(&hala->main_sem);
                continue;
            }

            hala->sprzedane_bilety_w_sektorze[sektor] += liczba_biletow;
            hala->sprzedane_bilety += liczba_biletow;
            obslugiwany_kibic->ma_bilet = 1;
            obslugiwany_kibic->liczba_biletow = liczba_biletow;
            sem_post(&obslugiwany_kibic->bilet_sem);
            obslugiwany_kibic->sektor = sektor;

            kasjer_logger.log(INFO, std::format("[Kasa {}] Kibic {} - {} bilet(ów), sektor {} (sprzedano: {}/{})",
                   id, obslugiwany_kibic->id, liczba_biletow, sektor,
                   hala->sprzedane_bilety, K_KIBICOW));

            if (obslugiwany_kibic->jest_dzieckiem && liczba_biletow == 1) {
                hala->dzieci_bez_opiekuna[hala->rozmiar_dzieci++] = id_obslugiwanego_kibica;
            }

            sem_post(&hala->main_sem);
            usleep(300000);
        } else {
            sem_post(&hala->main_sem);
            usleep(100000);
        }
    }
}

int main(int argc, char *argv[]) {
    int id_kasy = atoi(argv[1]);
    int shm_id = atoi(argv[2]);
    Logger kasjer_logger = Logger(std::format("kasjer_{}.log", id_kasy));
    if (argc < 3) {
        kasjer_logger.log(ERROR, std::format("Użycie: {} <id_kasy> <shm_id> <sem_name> {}", argv[0], argc));
        return 1;
    }



    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) {
        kasjer_logger.log(CRITICAL, "shmat error");
        return 1;
    }
    kasjer_logger.log(INFO, std::format("[Kasa {}] Podłączono do pamięci współdzielonej", id_kasy));
    proces_kasy(id_kasy, hala, kasjer_logger);

    return 0;
}
