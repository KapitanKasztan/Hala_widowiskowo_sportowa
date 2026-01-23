#include <filesystem>
#include <format>

#include "../include/common.h"


void proces_kibica_z_kontrola(int moj_idx, Kibic *kibic, Hala *hala) {
    std::string logs_dir = "tmp_kibic_logs";
    std::error_code ec;
    std::filesystem::create_directories(logs_dir, ec);
    if (ec) {
        fprintf(stderr, "Failed to create logs directory `%s`: %s\n", logs_dir.c_str(), ec.message().c_str());
    }
    std::string logger_filename = std::format("{}/kibic_{}.log", logs_dir, kibic->id);
    Logger kibic_logger(logger_filename); // Create logger instance
    sem_wait(&hala->main_sem);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = moj_idx;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post(&hala->main_sem);
    kibic_logger.log(INFO, std::format("[Kibic {}] Czekam w kolejce do kasy (pozycja: {}){}\n", kibic->id, pozycja_kasa, kibic->jest_dzieckiem ? " [DZIECKO]" : ""));
    // Czekaj na biletjak
    if (hala->sprzedane_bilety >= K_KIBICOW) {
        kibic_logger.log(INFO,"Brak biletów. Odchodze!\n");
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

    kibic_logger.log(INFO, std::format("[Kibic {}] Mam {} bilet(y), idę do sektora {}\n", kibic->id, kibic->liczba_biletow, kibic->sektor));

    // Idź do wejścia sektora
    sem_wait(&hala->main_sem);
    WejscieDoSektora *wejscie = &hala->wejscia[kibic->sektor];
    wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = moj_idx;
    int pozycja = wejscie->rozmiar_kolejki;  // Pozycja to rozmiar po dodaniu

    // Jeśli kibic ma towarzysza (kupił 2 bilety) - dodaj go też
    if (kibic->liczba_biletow > 1 && kibic->towarzysz != nullptr) {
        int tow_idx = kibic->towarzysz - hala->kibice;  // Oblicz indeks towarzysza
        wejscie->kolejka_do_kontroli[wejscie->rozmiar_kolejki++] = tow_idx;

        if (kibic->jest_dzieckiem) {
            kibic_logger.log(INFO, std::format("[Dziecko {}] Wchodzę z opiekunem {}\n", kibic->id, kibic->towarzysz->id));
        }
    }
    sem_post(&hala->main_sem);

    // Czekaj na przejście kontroli
    sem_wait(&kibic->na_hali_sem);

    kibic_logger.log(INFO, std::format("[Kibic {}] Jestem na hali w sektorze {}!\n", kibic->id, kibic->sektor));
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <idx> <shm_id>\n", argv[0]);
        return 1;
    }
    int idx = atoi(argv[1]);
    int shm_id = atoi(argv[2]);
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) {
        perror("shmat");
        return 1;
    }
    Kibic *kibic = &hala->kibice[idx];
    proces_kibica_z_kontrola(idx, kibic, hala);
    return 0;
}