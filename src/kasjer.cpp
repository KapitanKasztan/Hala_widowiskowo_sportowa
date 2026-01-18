#include <algorithm>

#include "../include/common.h"

bool sprawdz_czy_zamknac_kase_bilety(int id, Hala *hala) {
    if (hala->sprzedane_bilety >= K_KIBICOW) {
        printf("[Kasa %d] Wszystkie bilety sprzedane - zamykam\n", id);
        hala->otwarte_kasy--;
        return true;
    }
    return false;
}

bool obsluz_vip(int id, Hala *hala) {
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
        printf("[Kasa %d] VIP %d - brak miejsc w sektorze VIP\n", id, obslugiwany_kibic->id);
        return true;
    }
    hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] += liczba_biletow;
    //hala->sprzedane_bilety += liczba_biletow;
    printf("[Kasa %d] VIP - sprzedano %d bilet(ów) do sektora VIP\n", id, liczba_biletow);
    obslugiwany_kibic->ma_bilet = 1;
    obslugiwany_kibic->liczba_biletow = liczba_biletow;
    return true;
}

bool sprawdz_czy_zamknac_kase_kolejka(int id, Hala *hala) {
    // Zamknięcie kasy jeśli mało kibiców w kolejce (ale nie gdy są kibice do obsłużenia)
    if (hala->rozmiar_kolejki_kasy < (K_KIBICOW / LICZBA_KAS) * (hala->otwarte_kasy - 1)
        && hala->otwarte_kasy > 2
        && id == hala->otwarte_kasy) {
        hala->otwarte_kasy--;
        printf("[Kasa %d] Zamykam (za mało kibiców, otwarte: %d)\n", id, hala->otwarte_kasy);
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

int znajdz_wolny_sektor(Hala *hala, int &liczba_biletow, int id_kibica) {
    int sektor = rand() % 8;
    int proby = 0;
    while (hala->sprzedane_bilety_w_sektorze[sektor] + liczba_biletow > K_KIBICOW / 8) {
        if (liczba_biletow == 0) {
            return -1;
        }
        if (proby == 8) {
            liczba_biletow--;
            printf("[Kasa] Zmniejszam liczbę biletów dla [Kibica %d] do %d z powodu braku miejsc\n", id_kibica, liczba_biletow);
            proby = 0;
        }
        sektor = (sektor + 1) % 8;
        proby++;
    }
    return sektor;
}

void proces_kasy(int id, Hala *hala, sem_t *kasa_sem) {
    printf("[Kasa %d] Otwarcie\n", id);

    while (1) {

        sem_wait(kasa_sem);

        if (sprawdz_czy_zamknac_kase_bilety(id, hala)) {
            sem_post(kasa_sem);
            exit(0);
        }

        if (obsluz_vip(id, hala)) {
            sem_post(kasa_sem);
            usleep(500000);
            continue;
        }

        if (sprawdz_czy_zamknac_kase_kolejka(id, hala)) {
            sem_post(kasa_sem);
            exit(0);
        }

        // Obsługa normalnego kibica
        if (hala->rozmiar_kolejki_kasy > 0) {
            int id_obslugiwanego_kibica = hala->kolejka_do_kasy[0];
            Kibic* obslugiwany_kibic = &hala->kibice[id_obslugiwanego_kibica];

            przesun_kolejke(hala);

            int liczba_biletow = 1;

            int sektor = znajdz_wolny_sektor(hala, liczba_biletow, obslugiwany_kibic->id);
            if (sektor == -1) {
                printf("[Kasa %d] Brak miejsc w żadnym sektorze dla kibica %d\n", id, obslugiwany_kibic->id);
                obslugiwany_kibic->ma_bilet = 0;
                przesun_kolejke(hala);
                sem_post(kasa_sem);
                continue;
            }


            hala->sprzedane_bilety_w_sektorze[sektor] += liczba_biletow;
            hala->sprzedane_bilety += liczba_biletow;
            obslugiwany_kibic->ma_bilet = 1;
            obslugiwany_kibic->liczba_biletow = liczba_biletow;
            obslugiwany_kibic->sektor = sektor;

            printf("[Kasa %d] Kibic %d - %d bilet(ów), sektor %d (sprzedano: %d/%d)\n",
                   id, obslugiwany_kibic->id, liczba_biletow, sektor,
                   hala->sprzedane_bilety, K_KIBICOW);

            if (obslugiwany_kibic->jest_dzieckiem && liczba_biletow == 1) {
                hala->dzieci_bez_opiekuna[hala->rozmiar_dzieci++] = id_obslugiwanego_kibica;
            }

            sem_post(kasa_sem);
            usleep(300000);
        } else {
            sem_post(kasa_sem);
            usleep(100000);
        }
    }
}

void generator_kas(Hala *hala, sem_t *k_sem) {
    printf("[Generator] Uruchamiam generowanie kas...\n");

    sem_wait(k_sem);
    for (int i = 0; i < 2; i++) {
        hala->otwarte_kasy++;
        int id_kasy = hala->otwarte_kasy;
        if (fork() == 0) {
            sem_post(k_sem);
            proces_kasy(id_kasy, hala, k_sem);
            exit(0);
        }
        printf("[Generator] Otwieram kasę %d\n", id_kasy);
    }
    sem_post(k_sem);

    while (1) {
        usleep(200000);

        sem_wait(k_sem);
        // Zakończ gdy wszystko sprzedane I kolejka pusta
        if (hala->sprzedane_bilety >= K_KIBICOW && hala->rozmiar_kolejki_kasy == 0) {
            sem_post(k_sem);
            break;
        }

        int wymagane_kasy = (hala->rozmiar_kolejki_kasy / (K_KIBICOW / 10)) + 1;
        if (wymagane_kasy < 2) wymagane_kasy = 2;
        if (wymagane_kasy > LICZBA_KAS) wymagane_kasy = LICZBA_KAS;

        if (hala->otwarte_kasy < wymagane_kasy) {
            hala->otwarte_kasy++;
            int id_kasy = hala->otwarte_kasy;
            if (fork() == 0) {
                sem_post(k_sem);
                proces_kasy(id_kasy, hala, k_sem);
                exit(0);
            }
            printf("[Generator] Otwieram kasę %d (kolejka: %d)\n", id_kasy, hala->rozmiar_kolejki_kasy);
        }
        sem_post(k_sem);
    }

    printf("[Generator] Koniec sprzedaży biletów\n");
}