# Temat 18 - Hala widowiskowo-sportowa

W hali widowiskowo-sportowej o pojemności K kibiców ma zostać rozegrany mecz finałowy siatkówki. Kibice mogą być rozmieszczeni w 8 sektorach, każdy o pojemności K/8. Dodatkowo na hali znajduje się sektor VIP.

## Zakup biletów

Zakup biletów odbywa się bezpośrednio przed zawodami. W hali jest łącznie 10 kas. Zasady ich działania przyjęte przez kierownika obiektu są następujące:

* Zawsze działają min. 2 stanowiska kasowe.
* Na każdych K/10 kibiców znajdujących się w kolejce do kasy powinno przypadać min. 1 czynne stanowisko kasowe.
* Jeśli liczba kibiców w kolejce jest mniejsza niż (K/10)\*(N-1), gdzie N oznacza liczbę czynnych kas, to jedna z kas zostaje zamknięta.
* Jeden kibic może kupić maksymalnie 2 bilety w tym samym sektorze.
* Bilety na miejsca w poszczególnych sektorach sprzedawane są losowo przez wszystkie kasy.
* Kasy są automatycznie zamykane po sprzedaży wszystkich miejscówek.
* Osoby VIP kupują bilet omijając kolejkę (mniej niż 0,3% \* K).

Kibice przychodzą do kas w losowych momentach czasu (nawet po rozpoczęciu spotkania). Mecz rozpoczyna się o godzinie Tp.

## Zasady bezpieczeństwa

Z uwagi na rangę imprezy ustalono następujące rygorystyczne zasady bezpieczeństwa:

* Do każdego z 8 sektorów jest osobne wejście.
* Wejście na halę możliwe będzie tylko po przejściu drobiazgowej kontroli, mającej zapobiec wnoszeniu przedmiotów niebezpiecznych.
* Kontrola przy każdym wejściu jest przeprowadzana równolegle na 2 stanowiskach, na każdym z nich mogą znajdować się równocześnie maksymalnie 3 osoby.
* Jeśli kontrolowana jest więcej niż 1 osoba równocześnie na stanowisku, to należy zagwarantować by byli to kibice tej samej drużyny.
* Kibic oczekujący na kontrolę może przepuścić w kolejce maksymalnie 5 innych kibiców. Dłuższe czekanie wywołuje jego frustrację i agresywne zachowanie, którego należy unikać za wszelką cenę.
* Istnieje niewielka liczba kibiców VIP (mniejsza niż 0,3% \* K), którzy wchodzą (i wychodzą) na halę osobnym wejściem i nie przechodzą kontroli bezpieczeństwa (muszą mieć bilet kupiony w kasie).
* Dzieci poniżej 15 roku życia wchodzą na stadion pod opieką osoby dorosłej.

## Sygnały sterujące

Po wydaniu przez kierownika polecenia:

* **Sygnał 1** - pracownik techniczny wstrzymuje wpuszczanie kibiców do danego sektora.
* **Sygnał 2** - pracownik techniczny wznawia wpuszczanie kibiców.
* **Sygnał 3** - wszyscy kibice opuszczają stadion. W momencie gdy wszyscy kibice opuścili dany sektor, pracownik techniczny wysyła informację do kierownika.

## Zadanie

Napisz odpowiednie programy kierownika hali, kasjera, pracownika technicznego i kibica. Raport z przebiegu symulacji zapisać w pliku (plikach) tekstowym.

### Linki do repozytorium i raportu:
* #### [Github](https://github.com/KapitanKasztan/Hala_widowiskowo_sportowa/tree/main)
* #### [Pełny Raport](https://mckpk-my.sharepoint.com/:w:/g/personal/jakub_olma_student_pk_edu_pl/IQBXUVkME4raQqQn15wa52yhARRmJmL4ohxkmCEfuLhrxpw?e=KgqeOB)

# Dokumentacja Techniczna: Elementy IPC i Testy Systemowe

Dokumentacja opisuje mechanizmy komunikacji międzyprocesowej (IPC) wykorzystane w projekcie "Hala widowiskowo-sportowa" oraz scenariusze testowe weryfikujące poprawność działania symulacji.

---

## 1. Elementy IPC w projekcie

### 1.1. Pamięć Dzielona (Shared Memory)
* **Funkcja:** Główny magazyn stanu aplikacji. Udostępnia strukturę `Hala` wszystkim procesom.
* **Zastosowanie:** Przechowuje liczniki (sprzedane bilety, osoby na hali), tablice kolejek oraz flagi stanu (ewakuacja, koniec meczu). Eliminuje konieczność ciągłego kopiowania danych między procesami.
* **Kluczowe funkcje:** `shmget`, `shmat`.

### 1.2. Tablica Semaforów
* **Funkcja:** Synchronizacja procesów i ochrona sekcji krytycznych (zapobieganie *race conditions*).

#### Lista Semaforów:
1.  **SEM_MAIN (Indeks 0)**
    * **Rola:** Główny semafor synchronizujący dostęp do całej struktury pamięci dzielonej (`Hala`). Chroni globalne liczniki (sprzedane bilety, liczba osób na hali) przed jednoczesnym zapisem przez wiele procesów.
2.  **SEM_KASY (Indeks 1)**
    * **Rola:** Semafor dedykowany operacjom kasowym (rzadziej używany bezpośrednio, gdyż `SEM_MAIN` przejmuje większość odpowiedzialności).
3.  **SEM_KOLEJKA (Indeks 2)**
    * **Rola:** Semafor do zarządzania operacjami na kolejkach oczekujących.
4.  **SEM_WEJSCIA (Indeksy 3–10)**
    * **Rola:** Zbiór **8 semaforów** (po jednym dla każdego z 8 sektorów).
    * **Działanie:** Każdy semafor chroni kolejkę wejściową (`WejscieDoSektora`) konkretnego sektora. Dzięki temu procesowanie wejścia w sektorze 1 nie blokuje wejścia do sektora 2.

### 1.3. Kolejka Komunikatów (Message Queue)
* **Zastosowanie:** Przekazywanie danych do konkretnego procesu.
    * Kasjer wysyła bilet ustawiając `mtype` na ID konkretnego kibica.
    * Bramka wysyła potwierdzenie wejścia.
    * Pozwala to na selektywny odbiór komunikatów (`msgrcv` czeka na konkretny typ).
* **Kluczowe funkcje:** `msgsnd`, `msgrcv`.

### 1.4. Sygnały (Unix Signals)
* **Funkcja:** Asynchroniczne sterowanie przepływem i przerwania.
* **Zastosowanie:** Realizacja logiki zarządczej Kierownika.
    * `SIGUSR1` / `SIGUSR2`: Wstrzymują/wznawiają pracę bramek.
    * `SIGRTMIN` (sygnał czasu rzeczywistego): Wymusza natychmiastową ewakuację, przerywając pętle procesów.

---

## 2. Scenariusze Testowe

### Test 1: Zombie (Czystość procesów po zakończeniu)

**Cel:** Sprawdzenie, czy uruchomienie i zakończenie programu nie pozostawia w systemie żadnych "śmieci" (procesów zombie lub wiszących zasobów).

* **Plik testowy:** `test_zombie.sh`
* **Zależności:** `tests_src/zombie.sh` (narzędzie pomocnicze).

**Przebieg testu:**
1.  Utworzenie katalogu na artefakty testowe (`test_artifacts`).
2.  **PRZED:** Uruchomienie skryptu pomocniczego, który zapisuje stan procesów do `before.txt`.
3.  Uruchomienie symulacji:
    ```bash
    ./Hala_widowiskowo_sportowa 5 20
    ```
4.  **PO:** Ponowne uruchomienie skryptu pomocniczego i zapisanie stanu do `after.txt`.
5.  **Weryfikacja:** Porównanie plików za pomocą `diff`.

**Warunek zaliczenia:**
* **PASS:** Pliki identyczne.
* **FAIL:** Wykryto różnice (wypisane na ekran).

---

### Test 2: Spójność Danych i Race Conditions

**Cel:** Weryfikacja logiki biznesowej (sprzedane bilety == osoby na hali) oraz stabilności pamięci.

* **Plik testowy:** `test_spojnosci.sh`
* **Logi:** `tests/tmp/race_test.log`

**Przebieg testu:**
1.  Uruchomienie symulacji z przekierowaniem `stdout` i `stderr` do logu.
2.  Pobranie danych z logów:
    * `Sprzedane:` (Liczba sprzedanych biletów)
    * `Na hali:` (Liczba kibiców wewnątrz)
3.  Analiza błędów krytycznych (grep fraz: `Segmentation fault`, `core dumped`, `mismatch`).

**Warunek zaliczenia:**
Test zwraca **PASS**, jeśli:
1.  Różnica między liczbą biletów a liczbą osób wynosi **0**.
2.  Liczba błędów pamięci wynosi **0**.

---

### Test 3: Zarządzanie Zasobami IPC (Memory Leaks)

**Cel:** Weryfikacja zwolnienia pamięci dzielonej, semaforów i kolejek komunikatów.

* **Plik testowy:** `test_czyszczenia_ipc.sh`

**Przebieg testu:**
1.  **PRZED:** Zliczenie zasobów IPC użytkownika:
    ```bash
    ipcs -m -s -q > tmp/before.log
    ```
2.  Uruchomienie i zakończenie symulacji.
3.  **PO:** Ponowne zliczenie zasobów do `tmp/after.log`.
4.  **Weryfikacja:** Porównanie list zasobów.

> **Uwaga:** Test opiera się na porównaniu różnic (`diff`), a nie na warunku `count == 0`, ponieważ środowiska programistyczne (np. CLion) mogą tworzyć własne segmenty pamięci dzielonej, co powodowałoby fałszywe błędy.

**Warunek zaliczenia:**
* **PASS:** Brak różnic w zasobach IPC przed i po symulacji.

---

### Test 4: Czyszczenie procesów przy przerwaniu (SIGINT)

**Cel:** Weryfikacja, czy główny proces (Kierownik) poprawnie zabija procesy potomne po otrzymaniu sygnału `Ctrl+C`.

* **Plik testowy:** `test_czyszczenia_procesow.sh`

**Przebieg testu:**
1.  Uruchomienie symulacji w tle (`&`) na 60 sekund.
2.  Odczekanie 5 sekund na start procesów potomnych.
3.  **Wymuszenie zatrzymania:**
    ```bash
    kill -SIGINT <PID_GLOWNEGO_PROCESU>
    ```
4.  Oczekiwanie (`wait`) na zakończenie procesu głównego.
5.  **Sprawdzenie sierot:** Przeszukanie listy procesów (`pgrep`) pod kątem nazw: `kasjer`, `kibic`, `kierownik`, `pracownik_techniczny`.

**Warunek zaliczenia:**
* **PASS:** Lista wiszących procesów jest pusta.
* **FAIL:** Skrypt wypisuje PID-y procesów, które przetrwały.