#include "../include/common.h"

void proces_kibica_vip(int id, Kibic *kibic ,Hala *hala, sem_t *sem_kasa, sem_t *sem_hala) {
    sem_wait(sem_kasa);
    hala->kolejka_do_kasy_VIP[hala->rozmiar_kolejki_kasy_vip++] = id;

    printf("[VIP %d] Żądam obsługi (VIP: %d)\n", id, hala->rozmiar_kolejki_kasy_vip);
    sem_post(sem_kasa);


    while (!kibic->ma_bilet) {
        usleep(100000);
        if (hala->sprzedane_bilety_w_sektorze[SEKTOR_VIP] >= POJEMNOSC_VIP) {
            printf("Brak biletów. Odchodze!\n");
            exit(0);
        }

    }
    printf("[VIP %d] Wchodzę osobnym wejściem do sektora VIP (bez kontroli)\n", id);

    sem_wait(sem_hala);
    hala->kibice_na_hali++;
    hala->kibice_w_sektorze[SEKTOR_VIP][hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]++] = kibic->id;
    sem_post(sem_hala);

    printf("[VIP %d] Jestem na hali!\n", id);
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
    while (!kibic->ma_bilet) {
        usleep(100000);
        if (hala->sprzedane_bilety >= K_KIBICOW) {
            printf("Brak biletów. Odchodze!\n");
            exit(0);
        }

    }

    // // Jeśli dziecko z 1 biletem - musi poczekać na opiekuna
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
    while (!kibic->na_hali) {
        usleep(200000);
    }
    printf("[Kibic %d] Jestem na hali w sektorze %d!\n", kibic->id, kibic->sektor);
}
