#include "../include/common.h"

void proces_kibica_vip(int id, Kibic *kibic ,Hala *hala, sem_t *sem) {
    sem_wait(sem);
    hala->vip_w_kolejce++;
    sem_post(sem);

    printf("[VIP %d] Żądam obsługi (VIP: %d)\n", id, hala->vip_w_kolejce);

    while (1) {
        sem_wait(sem);
        int vip_count = hala->vip_w_kolejce;
        sem_post(sem);

        if (vip_count == 0) break;
        sleep(1);
    }
    printf("[VIP %d] Wchodzę osobnym wejściem do sektora VIP (bez kontroli)\n", id);

    sem_wait(sem);
    hala->kibice_na_hali++;
    //hala->kibice_vip_na_hali_ids[];
    sem_post(sem);

    printf("[VIP %d] Jestem na hali!\n", id);
}

void proces_kibica_z_kontrola(int moj_idx, Kibic *kibic, Hala *hala, sem_t *sem) {
    // Znajdź indeks tego kibica
    //int moj_idx = kibic - hala->kibice;  // Oblicz indeks z wskaźnika

    sem_wait(sem);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = moj_idx;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post(sem);

    printf("[Kibic %d] Czekam w kolejce do kasy (pozycja: %d)%s\n", 
           kibic->id, pozycja_kasa, 
           kibic->jest_dzieckiem ? " [DZIECKO]" : "");

    // Czekaj na bilet
    while (!kibic->ma_bilet) {
        usleep(100000);
    }

    // Jeśli dziecko z 1 biletem - musi poczekać na opiekuna
    if (kibic->jest_dzieckiem && kibic->liczba_biletow == 1) {
        printf("[Dziecko %d] Czekam na opiekuna...\n", kibic->id);
        
        // Czekaj aż opiekun zostanie przypisany
        while (kibic->opiekun == nullptr) {
            usleep(200000);
        }
        printf("[Dziecko %d] Mój opiekun to kibic %d\n", kibic->id, kibic->opiekun->id);
    }

    printf("[Kibic %d] Mam %d bilet(y), idę do sektora %d\n",
           kibic->id, kibic->liczba_biletow, kibic->sektor);

    // Idź do wejścia sektora
    sem_wait(sem);
    WejscieDoSektora *wejscie = &hala->wejscia[kibic->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = moj_idx;
    int pozycja = wejscie->rozmiar_kolejki;  // Pozycja to rozmiar po dodaniu

    // Jeśli kibic ma towarzysza (kupił 2 bilety) - dodaj go też
    if (kibic->liczba_biletow == 2 && kibic->towarzysz != nullptr) {
        int tow_idx = kibic->towarzysz - hala->kibice;  // Oblicz indeks towarzysza
        wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = tow_idx;

        if (kibic->jest_dzieckiem) {
            printf("[Dziecko %d] Wchodzę z opiekunem %d\n", kibic->id, kibic->towarzysz->id);
        }
    }
    sem_post(sem);

    // Czekaj na przejście kontroli
    while (1) {
        sem_wait(sem);
        int w_kolejce = 0;
    for (int i = 0; i < wejscie->rozmiar_kolejki; i++) {
        if (wejscie->kolejka_do_kontroli[i] == moj_idx) {
                w_kolejce = 1;
                break;
            }
        }
        sem_post(sem);

        if (!w_kolejce) break;
        usleep(200000);
    }

    printf("[Kibic %d] Jestem na hali w sektorze %d!\n", kibic->id, kibic->sektor);
}

// void proces_kibica_vip_z_biletem(int id, int sektor, Hala *hala, sem_t *sem) {
//     sem_wait(sem);
//     hala->vip_w_kolejce++;
//     sem_post(sem);
//
//     printf("[VIP %d] Kupuję bilet w kasie\n", id);
//     sleep(1);
//
//
// }
