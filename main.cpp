#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include "include/common.h"


void proces_kasy(int id, Hala *hala, sem_t *sem) {
    printf("[Kasa %d] Otwarcie\n", id);
    while (1) {
        if (hala->sprzedane_bilety >= K_KIBICOW) {
            sem_post(sem);
            exit(0);
        }
        sem_wait(sem);

        // Sprawdź czy są VIP-owie (mają priorytet)
        if (hala->vip_w_kolejce > 0) {
            hala->vip_w_kolejce--;
            sem_post(sem);

            // Obsłuż VIP-a
            int liczba_biletow = (rand() % 3) + 1;
            if (liczba_biletow > 2) {
                printf("[Kasa %d] VIP - Nie mogę sprzedać więcej niż 2 bilety\n", id);
                liczba_biletow = 2;
            }

            printf("[Kasa %d] Obsługuję VIP-a - sprzedaję %d bilet(ów)\n", id, liczba_biletow);
            sleep(1);

            sem_wait(sem);
            if (hala->sprzedane_bilety + liczba_biletow <= K_KIBICOW) {
                hala->sprzedane_bilety += liczba_biletow;
            } else {
                int pozostale = K_KIBICOW - hala->sprzedane_bilety;
                if (pozostale > 0) {
                    hala->sprzedane_bilety += pozostale;
                    printf("[Kasa %d] VIP - Sprzedano tylko %d bilet(ów)\n", id, pozostale);
                }
            }
            sem_post(sem);
            continue;
        }

        // Reszta kodu obsługi normalnych kibiców pozostaje bez zmian
        if ((K_KIBICOW/10)*(hala->otwarte_kasy-1) > hala->kolejka_do_kasy && hala->otwarte_kasy > 2 && id == hala->otwarte_kasy) {
            hala->otwarte_kasy--;
            printf("[Kasa %d] Zamykam kasę (otwarte kasy: %d)\n", id, hala->otwarte_kasy);
            sem_post(sem);
            exit(0);
        }
        if (hala->kolejka_do_kasy > 0) {
            hala->kolejka_do_kasy--;
            int kibicow = hala->kolejka_do_kasy;
            sem_post(sem);

            // Losuj liczbę biletów (1 lub 2)
            int liczba_biletow = (rand() % 2) + 1;
            if (liczba_biletow > 2) {
                printf("[Kasa %d] Nie moge sprzedać więcej niż 2 bilety\n", id);
                liczba_biletow = 2;
            }
            printf("[Kasa %d] Obsługuję kibica - sprzedaję %d bilet(ów) (zostało w kolejce: %d)\n",
                   id, liczba_biletow, kibicow);
            sleep(1);

            sem_wait(sem);
            // Sprawdź czy można sprzedać tyle biletów
            if (hala->sprzedane_bilety + liczba_biletow <= K_KIBICOW) {
                hala->sprzedane_bilety += liczba_biletow;
            } else {
                // Sprzedaj tylko pozostałe bilety
                int pozostale = K_KIBICOW - hala->sprzedane_bilety;
                if (pozostale > 0) {
                    hala->sprzedane_bilety += pozostale;
                    printf("[Kasa %d] Sprzedano tylko %d bilet(ów) - koniec biletów\n", id, pozostale);
                }
            }
            sem_post(sem);
        } else {
            sem_post(sem);
            sleep(1);
        }
    }
}

void generator_kas(Hala *hala, sem_t *sem) {
    unsigned id;
    sem_wait(sem);
    for (id = 0; id < 2; id++) {
        hala->otwarte_kasy++;
        if (fork() == 0) {
            int id_kasy = hala->otwarte_kasy;
            proces_kasy(id_kasy, hala, sem);
            exit(0);
        }
        printf("Otwieram nową kasę (otwarte kasy: %d)\n", hala->otwarte_kasy);
        usleep(50000);
    }
    sem_post(sem);
    while (1) {
        sem_wait(sem);
        if (hala->sprzedane_bilety == K_KIBICOW) {
            sem_post(sem);
            exit(0);;
        }
        if (hala->kolejka_do_kasy > (K_KIBICOW/LICZBA_KAS)*hala->otwarte_kasy && hala->otwarte_kasy < LICZBA_KAS) {
            hala->otwarte_kasy++;
            printf("Otwieram nową kasę (otwarte kasy: %d)\n", hala->otwarte_kasy);
            if (fork() == 0) {
                int id_kasy = hala->otwarte_kasy;
                proces_kasy(id_kasy, hala, sem);
                sem_post(sem);
                exit(0);
            }
        }
        sem_post(sem);
        usleep(50000);  // Sprawdzaj co 0.5s

    }


}

void proces_kibica_vip(int id, Hala *hala, sem_t *sem) {
    //printf("[VIP %d] Idę bezpośrednio do kasy\n", id);

    sem_wait(sem);
    hala->vip_w_kolejce++;
    sem_post(sem);

    // VIP nie czeka w kolejce - od razu się zgłasza do kasy
    printf("[VIP %d] Żądam obsługi (VIP: %d)\n", id, hala->vip_w_kolejce);

    // Czekaj na obsługę przez kasę
    while (1) {
        sem_wait(sem);
        int vip_count = hala->vip_w_kolejce;
        sem_post(sem);

        if (vip_count == 0) break;  // Zostałem obsłużony
        sleep(1);
    }
}

void proces_stanowiska(int sektor_id, int stanowisko_id, Hala *hala, sem_t *sem) {
    printf("[Stanowisko %d-%d] Start kontroli\n", sektor_id, stanowisko_id);

    while (1) {
        sem_wait(sem);

        WejscieDoSektora *wejscie = &hala->wejscia[sektor_id];
        Stanowisko *stan = &wejscie->stanowiska[stanowisko_id];

        // Jeśli stanowisko jest pełne, czekaj
        if (stan->liczba_osob >= MAX_OSOB_NA_STANOWISKU) {
            sem_post(sem);
            usleep(100000);
            continue;
        }

        // Szukaj kibica do obsługi z kolejki
        int znaleziono = 0;
        for (int i = 0; i < wejscie->rozmiar_kolejki && !znaleziono; i++) {
            int kibic_id = wejscie->kolejka[i];
            int kibic_druzyna = wejscie->druzyny_w_kolejce[i];

            // Sprawdź czy kibic może wejść na stanowisko
            int moze_wejsc = 0;

            if (stan->liczba_osob == 0) {
                // Puste stanowisko - każdy może wejść
                moze_wejsc = 1;
            } else if (stan->druzyna_na_stanowisku == kibic_druzyna) {
                // Ta sama drużyna - może wejść
                moze_wejsc = 1;
            }

            if (moze_wejsc) {
                // Sprawdź ile osób przepuści ten kibic
                int przepuszczonych = i;  // Ile osób przed nim
                if (przepuszczonych > MAX_PRZEPUSZCZONYCH) {
                    // Kibic nie może czekać dłużej - frustracja!
                    printf("[!] Kibic %d jest sfrustrowany! (przepuścił %d osób)\n",
                           kibic_id, przepuszczonych);
                }

                // Dodaj do stanowiska
                stan->kibice_ids[stan->liczba_osob] = kibic_id;
                stan->liczba_osob++;
                if (stan->liczba_osob == 1) {
                    stan->druzyna_na_stanowisku = kibic_druzyna;
                }

                // Usuń z kolejki
                for (int j = i; j < wejscie->rozmiar_kolejki - 1; j++) {
                    wejscie->kolejka[j] = wejscie->kolejka[j + 1];
                    wejscie->druzyny_w_kolejce[j] = wejscie->druzyny_w_kolejce[j + 1];
                }
                wejscie->rozmiar_kolejki--;

                printf("[Stanowisko %d-%d] Kontroluję kibica %d (drużyna %c, osób na stanowisku: %d)\n",
                       sektor_id, stanowisko_id, kibic_id,
                       kibic_druzyna == DRUZYNA_A ? 'A' : 'B',
                       stan->liczba_osob);

                znaleziono = 1;
            }
        }

        sem_post(sem);

        // Symulacja czasu kontroli
        if (znaleziono) {
            usleep(500000 + rand() % 500000);  // 0.5-1s kontroli

            // Zakończ kontrolę pierwszego kibica na stanowisku
            sem_wait(sem);
            if (stan->liczba_osob > 0) {
                int kibic_id = stan->kibice_ids[0];

                // Przesuń pozostałych
                for (int j = 0; j < stan->liczba_osob - 1; j++) {
                    stan->kibice_ids[j] = stan->kibice_ids[j + 1];
                }
                stan->liczba_osob--;

                if (stan->liczba_osob == 0) {
                    stan->druzyna_na_stanowisku = -1;
                }

                hala->kibice_na_hali++;
                printf("[Stanowisko %d-%d] Kibic %d przeszedł kontrolę i wchodzi na halę\n",
                       sektor_id, stanowisko_id, kibic_id);
            }
            sem_post(sem);
        } else {
            usleep(100000);
        }
    }
}

void proces_kibica_z_kontrola(int id, int druzyna, int sektor, int jest_dzieckiem,
                               int id_opiekuna, Hala *hala, sem_t *sem) {
    // Najpierw kup bilet
    sem_wait(sem);
    hala->kolejka_do_kasy++;
    sem_post(sem);

    // Czekaj na bilet (uproszczone)
    sleep(1);

    // Idź do wejścia sektora
    printf("[Kibic %d] Idę do wejścia sektora %d (drużyna %c%s)\n",
           id, sektor, druzyna == DRUZYNA_A ? 'A' : 'B',
           jest_dzieckiem ? ", dziecko z opiekunem" : "");

    sem_wait(sem);
    WejscieDoSektora *wejscie = &hala->wejscia[sektor];

    // Dodaj do kolejki
    int pozycja = wejscie->rozmiar_kolejki;
    wejscie->kolejka[pozycja] = id;
    wejscie->druzyny_w_kolejce[pozycja] = druzyna;
    wejscie->przepuszczeni[pozycja] = 0;
    wejscie->rozmiar_kolejki++;

    // Jeśli dziecko, dodaj też opiekuna zaraz za nim
    if (jest_dzieckiem && id_opiekuna > 0) {
        pozycja = wejscie->rozmiar_kolejki;
        wejscie->kolejka[pozycja] = id_opiekuna;
        wejscie->druzyny_w_kolejce[pozycja] = druzyna;
        wejscie->rozmiar_kolejki++;
    }

    sem_post(sem);

    // Czekaj na przejście kontroli
    while (1) {
        sem_wait(sem);
        int na_hali = 0;
        // Sprawdź czy kibic jest już na hali (usunięty z kolejki)
        int w_kolejce = 0;
        for (int i = 0; i < wejscie->rozmiar_kolejki; i++) {
            if (wejscie->kolejka[i] == id) {
                w_kolejce = 1;
                break;
            }
        }
        sem_post(sem);

        if (!w_kolejce) break;
        usleep(200000);
    }
}

void proces_kibica_vip_z_biletem(int id, int sektor, Hala *hala, sem_t *sem) {
    // VIP najpierw kupuje bilet w kasie
    sem_wait(sem);
    hala->vip_w_kolejce++;
    sem_post(sem);

    printf("[VIP %d] Kupuję bilet w kasie\n", id);
    sleep(1);

    // VIP wchodzi osobnym wejściem bez kontroli
    printf("[VIP %d] Wchodzę osobnym wejściem do sektora %d (bez kontroli)\n", id, sektor);

    sem_wait(sem);
    hala->kibice_na_hali++;
    sem_post(sem);

    printf("[VIP %d] Jestem na hali!\n", id);
}


int main() {
    srand(time(NULL));

    // Inicjalizacja pamięci współdzielonej
    int shm_id = shmget(IPC_PRIVATE, sizeof(Hala), IPC_CREAT | 0666);
    Hala *hala = (Hala*) shmat(shm_id, NULL, 0);

    // Inicjalizacja struktury
    hala->kolejka_do_kasy = 0;
    hala->sprzedane_bilety = 0;
    hala->otwarte_kasy = 0;
    hala->vip_w_kolejce = 0;
    hala->kibice_na_hali = 0;
    hala->czas_meczu = 0;

    // Inicjalizacja wejść do sektorów
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        hala->wejscia[s].rozmiar_kolejki = 0;
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            hala->wejscia[s].stanowiska[st].liczba_osob = 0;
            hala->wejscia[s].stanowiska[st].druzyna_na_stanowisku = -1;
        }
    }

    sem_t *sem = sem_open("/hala_sem", O_CREAT, 0666, 1);

    // Uruchom kasy
    if (fork() == 0) {
        generator_kas(hala, sem);
        exit(0);
    }

    // Uruchom stanowiska kontroli dla każdego sektora
    for (int s = 0; s < LICZBA_SEKTOROW; s++) {
        for (int st = 0; st < STANOWISKA_NA_SEKTOR; st++) {
            if (fork() == 0) {
                proces_stanowiska(s, st, hala, sem);
                exit(0);
            }
        }
    }

    // Generuj kibiców (przychodzą w losowych momentach)
    for (int i = 0; i < K_KIBICOW; i++) {
        if (fork() == 0) {
            int druzyna = rand() % 2;
            int sektor = rand() % LICZBA_SEKTOROW;
            int jest_dzieckiem = (rand() % 100) < 10;  // 10% dzieci
            int id_opiekuna = jest_dzieckiem ? (1000 + i) : 0;

            proces_kibica_z_kontrola(i + 1, druzyna, sektor, jest_dzieckiem,
                                      id_opiekuna, hala, sem);
            exit(0);
        }
        usleep(50000 + rand() % 100000);  // Losowy czas między kibicami
    }

    // Generuj VIP-ów (< 0.3% * K)
    int liczba_vip = (K_KIBICOW * 3) / 1000;
    if (liczba_vip < 1) liczba_vip = 1;

    for (int i = 0; i < liczba_vip; i++) {
        if (fork() == 0) {
            int sektor = rand() % LICZBA_SEKTOROW;
            proces_kibica_vip_z_biletem(i + 2000, sektor, hala, sem);
            exit(0);
        }
        usleep(500000);
    }

    // Symuluj start meczu po czasie Tp
    sleep(5);  // Tp = 5 sekund dla testu
    hala->czas_meczu = 1;
    printf("\n=== MECZ ROZPOCZĘTY ===\n");

    // Czekaj na zakończenie
    while (hala->kibice_na_hali < K_KIBICOW + liczba_vip) {
        sleep(1);
    }

    printf("\n=== PODSUMOWANIE ===\n");
    printf("Kibice na hali: %d\n", hala->kibice_na_hali);
    printf("Sprzedane bilety: %d\n", hala->sprzedane_bilety);

    system("pkill -P $$");
    shmdt(hala);
    shmctl(shm_id, IPC_RMID, NULL);
    sem_close(sem);
    sem_unlink("/hala_sem");

    return 0;
}
