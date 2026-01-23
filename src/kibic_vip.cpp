#include <filesystem>
#include <format>
#include "../include/common.h"
#include "../include/logger.h"

void proces_kibica_vip(int id, Kibic *kibic ,Hala *hala) {
    std::string logs_dir = "tmp_VIP_kibic_logs";
    std::error_code ec;
    std::filesystem::create_directories(logs_dir, ec);
    if (ec) {
        fprintf(stderr, "Failed to create logs directory `%s`: %s\n", logs_dir.c_str(), ec.message().c_str());
    }
    std::string logger_filename = std::format("{}/kibic_{}.log", logs_dir, kibic->id);
    Logger VIP_logger(logger_filename); // Create logger instance
    sem_wait(&hala->main_sem);
    hala->kolejka_do_kasy[hala->rozmiar_kolejki_kasy++] = id;
    int pozycja_kasa = hala->rozmiar_kolejki_kasy;
    sem_post(&hala->main_sem);

    sem_wait(&hala->main_sem);
    hala->kolejka_do_kasy_VIP[hala->rozmiar_kolejki_kasy_vip++] = id;

    VIP_logger.log(INFO, std::format("[VIP {}] Żądam obsługi (VIP: {})", id, hala->rozmiar_kolejki_kasy_vip));
    sem_post(&hala->main_sem);

    if (hala->sprzedane_bilety >= K_KIBICOW) {
        VIP_logger.log(WARNING, "[VIP] Brak biletów. Odchodzę!");
        exit(0);
    }
    sem_wait(&kibic->bilet_sem);
    VIP_logger.log(INFO, std::format("[VIP {}] Wchodzę osobnym wejściem do sektora VIP (bez kontroli)", id));

    sem_wait(&hala->main_sem);

    hala->kibice_na_hali++;
    hala->kibice_w_sektorze[SEKTOR_VIP][hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]++] = kibic->id;
    VIP_logger.log(INFO, std::format("[VIP] kibice na hali: {} w sektorze: {}", hala->kibice_na_hali, hala->kibice_w_sektorze_ilosc[SEKTOR_VIP]));

    sem_post(&hala->main_sem);

    VIP_logger.log(INFO, std::format("[VIP {}] Jestem na hali!", id));
}

int main(int argc, char *argv[]) {
    string logger_filename = "VIP_logger.log";
    Logger VIP_logger(logger_filename);

    if (argc < 3) {
        VIP_logger.log(ERROR, std::format("Użycie: {} <idx> <shm_id>", argv[0]));
        return 1;
    }
    int idx = atoi(argv[1]);
    int shm_id = atoi(argv[2]);
    Hala* hala = (Hala*)shmat(shm_id, NULL, 0);
    if (hala == (void*)-1) {
        VIP_logger.log(CRITICAL, "shmat error");
        return 1;
    }
    Kibic *kibic_vip = &hala->kibice_vip[idx];
    proces_kibica_vip(idx, kibic_vip, hala);
    return 0;
}
