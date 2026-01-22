#include <format>

#include "../include/common.h"

void proces_kibica_vip(int id, Kibic *kibic ,Hala *hala, sem_t *sem_kasa, sem_t *sem_hala) {
    sem_wait(sem_kasa);
    hala->kolejka_do_kasy_VIP[hala->rozmiar_kolejki_kasy_vip++] = id;

    printf("[VIP %d] Żądam obsługi (VIP: %d)\n", id, hala->rozmiar_kolejki_kasy_vip);
    sem_post(sem_kasa);


    if (hala->sprzedane_bilety >= K_KIBICOW) {
        printf("Brak biletów. Odchodze!\n");
        exit(0);
    }
    sem_wait(&kibic->bilet_sem);
    printf("[VIP %d] Wchodzę osobnym wejściem do sektora VIP (bez kontroli)\n", id);

    sem_wait(sem_hala);

    string logger_filename = "VIP_logger.log";
    Logger VIP_logger(logger_filename); // Create logger instance
    //VIP_logger.log(INFO, format("[VIP] kibice na hali przed {}", hala->kibice_na_hali));

    hala->kibice_na_hali++;

    hala->kibice_w_sektorze[SEKTOR_VIP][hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]++] = kibic->id;
    VIP_logger.log(INFO, format("[VIP] kibice na hali: {} w sektorze: {}", hala->kibice_na_hali, hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]));

    sem_post(sem_hala);

    printf("[VIP %d] Jestem na hali!\n", id);
}