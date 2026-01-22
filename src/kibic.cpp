#include <format>

#include "../include/common.h"

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


void proces_kibica_z_kontrola(int moj_idx, Kibic *kibic, Hala *hala, sem_t *kasa_sem, sem_t *hala_sem) {
    sem_wait(kasa_sem);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = moj_idx;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post(kasa_sem);

    printf("[Kibic %d] Czekam w kolejce do kasy (pozycja: %d)%s\n",
           kibic->id, pozycja_kasa,
           kibic->jest_dzieckiem ? " [DZIECKO]" : "");

    // Czekaj na biletjak
    if (hala->sprzedane_bilety >= K_KIBICOW) {
        printf("Brak biletów. Odchodze!\n");
        exit(0);
    }
    sem_wait(&kibic->bilet_sem);

    // Jeśli dziecko z 1 biletem - musi poczekać na opiekuna
    // if (kibic->jest_dzieckiem && kibic->liczba_biletow == 1) {
    //     printf("[Dziecko %d] Czekam na opiekuna...\n", kibic->id);
    //
    //     // Czekaj aż opiekun zostanie przypisany
    //     while (kibic->opiekun == nullptr) {
    //         usleep(200000);
    //     }
    //     printf("[Dziecko %d] Mój opiekun to kibic %d\n", kibic->id, kibic->opiekun->id);
    // }

    printf("[Kibic %d] Mam %d bilet(y), idę do sektora %d\n",
           kibic->id, kibic->liczba_biletow, kibic->sektor);

    // Idź do wejścia sektora
    sem_wait(hala_sem);
    WejscieDoSektora *wejscie = &hala->wejscia[kibic->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = moj_idx;
    int pozycja = wejscie->rozmiar_kolejki;  // Pozycja to rozmiar po dodaniu

    // Jeśli kibic ma towarzysza (kupił 2 bilety) - dodaj go też
    if (kibic->liczba_biletow > 1 && kibic->towarzysz != nullptr) {
        int tow_idx = kibic->towarzysz - hala->kibice;  // Oblicz indeks towarzysza
        wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = tow_idx;

        if (kibic->jest_dzieckiem) {
            printf("[Dziecko %d] Wchodzę z opiekunem %d\n", kibic->id, kibic->towarzysz->id);
        }
    }
    sem_post(hala_sem);

    // Czekaj na przejście kontroli
    sem_wait(&kibic->na_hali_sem);

    printf("[Kibic %d] Jestem na hali w sektorze %d!\n", kibic->id, kibic->sektor);
}
