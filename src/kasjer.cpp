#include "../include/common.h"

void proces_kasy(int id, Hala *hala, sem_t *sem) {
    printf("[Kasa %d] Otwarcie\n", id);

    while (1) {
        sem_wait(sem);

        // Sprawdź czy wszystko sprzedane I kolejka pusta
        if (hala->sprzedane_bilety >= K_KIBICOW && hala->rozmiar_kolejki_kasy == 0) {
            printf("[Kasa %d] Wszystkie bilety sprzedane - zamykam\n", id);
            hala->otwarte_kasy--;
            sem_post(sem);
            exit(0);
        }

        // Priorytet dla VIP-ów
        if (hala->vip_w_kolejce > 0) {
            hala->vip_w_kolejce--;

            int sektor = 8; // VIP-owie idą do sektora VIP
            int liczba_biletow = 0;

            if (hala->bilety_w_sektorze[sektor] + liczba_biletow <= K_KIBICOW / 8) {
                hala->bilety_w_sektorze[sektor] += liczba_biletow;
                hala->sprzedane_bilety += liczba_biletow;
                printf("[Kasa %d] VIP - sprzedano %d bilet(ów) do sektora VIP\n", id, liczba_biletow);
            } else {
                printf("[Kasa %d] VIP - brak miejsc w sektorze VIP\n", id);
            }
            sem_post(sem);
            usleep(500000);
            continue;
        }

        // Zamknięcie kasy jeśli mało kibiców w kolejce (ale nie gdy są kibice do obsłużenia)
        if (hala->rozmiar_kolejki_kasy < (K_KIBICOW / 10) * (hala->otwarte_kasy - 1)
            && hala->otwarte_kasy > 2
            && id == hala->otwarte_kasy
            && hala->sprzedane_bilety < K_KIBICOW) {  // Dodano warunek
            hala->otwarte_kasy--;
            printf("[Kasa %d] Zamykam (za mało kibiców, otwarte: %d)\n", id, hala->otwarte_kasy);
            sem_post(sem);
            exit(0);
        }

        // Obsługa normalnego kibica
        if (hala->rozmiar_kolejki_kasy > 0) {
            // Sprawdź czy są jeszcze bilety PRZED wyjęciem kibica z kolejki
            if (hala->sprzedane_bilety >= K_KIBICOW) {
                // Brak biletów - usuń kibica z kolejki i powiadom go
                int idx = hala->kolejka_do_kasy[0];
                Kibic* kibic = &hala->kibice[idx];

                // Przesuń kolejkę
                for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++) {
                    hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
                }
                hala->rozmiar_kolejki_kasy--;

                kibic->ma_bilet = 0;  // Oznacz że nie dostał biletu
                printf("[Kasa %d] Kibic %d - brak biletów, odchodzi\n", id, kibic->id);

                sem_post(sem);
                usleep(100000);
                continue;
            }

            int idx = hala->kolejka_do_kasy[0];
            Kibic* kibic = &hala->kibice[idx];

            // Przesuń kolejkę
            for (int j = 0; j < hala->rozmiar_kolejki_kasy - 1; j++) {
                hala->kolejka_do_kasy[j] = hala->kolejka_do_kasy[j + 1];
            }
            hala->rozmiar_kolejki_kasy--;

            // LOSOWY SEKTOR
            int sektor = rand() % 8;
            int proby = 0;
            while (hala->bilety_w_sektorze[sektor] >= K_KIBICOW / 8 && proby < 8) {
                sektor = (sektor + 1) % 8;
                proby++;
            }

            if (proby >= 8) {
                printf("[Kasa %d] Brak miejsc w żadnym sektorze dla kibica %d\n", id, kibic->id);
                kibic->ma_bilet = 0;
                sem_post(sem);
                continue;
            }

            int liczba_biletow = 1;

            // Sprawdź dostępność w sektorze
            if (hala->bilety_w_sektorze[sektor] + liczba_biletow > K_KIBICOW / 8) {
                liczba_biletow = K_KIBICOW / 8 - hala->bilety_w_sektorze[sektor];
            }

            // Sprawdź globalny limit
            if (hala->sprzedane_bilety + liczba_biletow > K_KIBICOW) {
                liczba_biletow = K_KIBICOW - hala->sprzedane_bilety;
            }

            if (liczba_biletow > 0) {
                hala->bilety_w_sektorze[sektor] += liczba_biletow;
                hala->sprzedane_bilety += liczba_biletow;
                kibic->ma_bilet = 1;
                kibic->liczba_biletow = liczba_biletow;
                kibic->sektor = sektor;

                printf("[Kasa %d] Kibic %d - %d bilet(ów), sektor %d (sprzedano: %d/%d)\n",
                       id, kibic->id, liczba_biletow, sektor,
                       hala->sprzedane_bilety, K_KIBICOW);

                if (kibic->jest_dzieckiem && liczba_biletow == 1) {
                    hala->dzieci_bez_opiekuna[hala->rozmiar_dzieci++] = idx;
                }
            } else {
                kibic->ma_bilet = 0;
                printf("[Kasa %d] Kibic %d - brak biletów\n", id, kibic->id);
            }

            sem_post(sem);
            usleep(300000);
        } else {
            sem_post(sem);
            usleep(100000);
        }
    }
}

void generator_kas(Hala *hala, sem_t *sem) {
    sem_wait(sem);
    for (int i = 0; i < 2; i++) {
        hala->otwarte_kasy++;
        int id_kasy = hala->otwarte_kasy;
        if (fork() == 0) {
            sem_post(sem);
            proces_kasy(id_kasy, hala, sem);
            exit(0);
        }
        printf("[Generator] Otwieram kasę %d\n", id_kasy);
    }
    sem_post(sem);

    while (1) {
        usleep(200000);

        sem_wait(sem);
        // Zakończ gdy wszystko sprzedane I kolejka pusta
        if (hala->sprzedane_bilety >= K_KIBICOW && hala->rozmiar_kolejki_kasy == 0) {
            sem_post(sem);
            break;
        }

        int wymagane_kasy = (hala->rozmiar_kolejki_kasy / (K_KIBICOW / 10)) + 1;
        if (wymagane_kasy < 2) wymagane_kasy = 2;
        if (wymagane_kasy > LICZBA_KAS) wymagane_kasy = LICZBA_KAS;

        if (hala->otwarte_kasy < wymagane_kasy) {
            hala->otwarte_kasy++;
            int id_kasy = hala->otwarte_kasy;
            if (fork() == 0) {
                sem_post(sem);
                proces_kasy(id_kasy, hala, sem);
                exit(0);
            }
            printf("[Generator] Otwieram kasę %d (kolejka: %d)\n", id_kasy, hala->rozmiar_kolejki_kasy);
        }
        sem_post(sem);
    }

    printf("[Generator] Koniec sprzedaży biletów\n");
}