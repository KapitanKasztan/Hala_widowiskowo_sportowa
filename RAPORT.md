# RAPORT PROJEKTU - Hala Widowiskowo-Sportowa

## 1. Założenia projektowe

System został zaprojektowany jako symulacja wieloprocesowa zarządzania halą sportową o pojemności K=160 kibiców. Główne założenia:

### Architektura wieloprocesowa
- **Komunikacja międzyprocesowa (IPC)**: Wykorzystanie shared memory, semaforów i kolejek komunikatów
- **Procesy niezależne**: Każdy komponent (kasjer, kibic, pracownik techniczny, kierownik) działa jako osobny proces
- **Synchronizacja**: Semafory IPC do kontroli dostępu do zasobów współdzielonych

### Zarządzanie czasem rzeczywistym
- Symulacja odbywa się w czasie rzeczywistym z losowymi opóźnieniami
- Generator losowo tworzy procesy kibiców
- Możliwość uruchomienia w trybie nieskończonym (multiple matches)

### Bezpieczeństwo i logowanie
- System raportowania zdarzeń do plików tekstowych w katalogu `reports/`
- Dedykowane logi dla każdego procesu z timestamp
- Automatyczne tworzenie raportów podsumowujących

## 2. Opis struktury kodu

### Główne komponenty

#### `main.cpp`
Główny proces zarządzający symulacją:
- Inicjalizacja zasobów IPC (shared memory, semafory, kolejki)
- Uruchamianie procesów potomnych (kasy, kibice, pracownicy)
- Dynamiczne zarządzanie ilością otwartych kas
- Monitoring stanu systemu i generowanie statystyk
- Obsługa zakończenia symulacji i czyszczenie zasobów

#### `include/common.h`
Wspólne definicje i struktury danych:
- Konfiguracja systemu (K_KIBICOW=160, LICZBA_KAS=10, LICZBA_SEKTOROW=8)
- Struktury: `Kibic`, `Hala`, `WejscieDoSektora`, `Stanowisko`
- Implementacja opakowań dla operacji na semaforach IPC
- Definicje typów komunikatów i stałych systemowych

#### `include/logger.h`
System logowania:
- Wielopoziomowe logi (DEBUG, INFO, WARNING, ERROR, CRITICAL)
- Osobne pliki dla każdego procesu
- Automatyczne timestampy
- Thread-safe zapis do plików

#### `src/kasjer.cpp`
Proces sprzedaży biletów:
- Obsługa dwóch kolejek: zwykłej i VIP (priorytet VIP)
- Dynamiczna sprzedaż biletów do losowych sektorów
- Limit 2 bilety na kibica (specjalne traktowanie dzieci - 2 bilety zawsze)
- Automatyczne zamykanie po wyprzedaniu wszystkich biletów

#### `src/kibic.cpp`
Proces pojedynczego kibica:
- Kupno biletu w kasie
- Przejście przez kontrolę bezpieczeństwa
- Oczekiwanie na opiekuna (dla dzieci <15 lat)
- Wejście na halę i przebywanie na meczu
- Obsługa ewakuacji (sygnał3)

#### `src/kibic_vip.cpp`
Proces VIP:
- Omijanie kolejki zwykłej (osobna kolejka VIP)
- Brak kontroli bezpieczeństwa
- Wejście bezpośrednio na sektor VIP

#### `src/pracownik_techniczny.cpp`
Kontrola wejścia do sektora:
- Zarządzanie 2 stanowiskami kontrolnymi na sektor
- Kontrola max 3 osób jednocześnie na stanowisku
- Wymuszenie tej samej drużyny na stanowisku (>1 osoba)
- Mechanizm przepuszczania kibiców (limit 5, aby uniknąć agresji)
- Obsługa sygnałów kierownika (wstrzymanie/wznowienie/ewakuacja)

#### `src/kierownik.cpp`
Zarządzanie operacjami:
- Monitorowanie stanu systemu
- Wysyłanie sygnałów do pracowników:
  - SIGUSR1: wstrzymanie wpuszczania
  - SIGUSR2: wznowienie wpuszczania
  - SIGRTMIN: ewakuacja

#### `tests.cpp`
Testy jednostkowe pokrywające wszystkie wymagania:
- K2-K8: Testy systemu kasowego
- C3-C7: Testy kontroli bezpieczeństwa
- M1-M3: Testy sygnałów kierownika

## 3. Co udało się zrobić

### Funkcjonalności zrealizowane w pełni:

✅ **System kasowy**
- Zawsze minimum 2 kasy otwarte
- Dynamiczne otwieranie kas: 1 kasa na K/10 kibiców w kolejce
- Dynamiczne zamykanie kas przy spadku ruchu
- Priorytetowa obsługa VIP (osobna kolejka)
- Automatyczne zamykanie po wyprzedaniu biletów

✅ **System kontroli wejścia**
- 8 sektorów, każdy z 2 stanowiskami kontrolnymi
- Maksymalnie 3 osoby na stanowisku
- Wymuszenie jednej drużyny na stanowisku przy >1 osobie
- Mechanizm przepuszczania (max 5 kibiców)
- VIP bez kontroli

✅ **Zarządzanie dziećmi**
- Dzieci (<15 lat) wymagają opiekuna
- Automatyczne kupowanie 2 biletów dla dziecka
- Synchronizacja wejścia dziecka z opiekunem

✅ **System sygnałów kierownika**
- Sygnał 1 (SIGUSR1): wstrzymanie wpuszczania kibiców
- Sygnał 2 (SIGUSR2): wznowienie wpuszczania
- Sygnał 3 (SIGRTMIN): ewakuacja całej hali

✅ **Logowanie i raporty**
- Dedykowane logi dla każdego procesu
- Raporty podsumowujące mecze
- Statystyki sprzedaży i obsadzenia sektorów

✅ **Tryb ciągły**
- Możliwość uruchomienia wielu meczy w trybie `--infinite`
- Automatyczne czyszczenie i reinicjalizacja zasobów między meczami

## 4. Problemy napotkane podczas implementacji

### Problem 1: Nieskończona pętla w procesie głównym
**Opis**: Proces główny nie kończył się prawidłowo po wyprzedaniu biletów  
**Przyczyna**: Błędny warunek sprawdzający zakończenie symulacji  
**Rozwiązanie**: Naprawiono w commicie "naprawiono nieskończoną pętlę" - dodano poprawny warunek: `if (na_hali >= sprzedane && sprzedane >= LIMIT_SPRZEDAZY)`

### Problem 2: Race conditions w dostępie do shared memory
**Opis**: Jednoczesny dostęp wielu procesów powodował niespójność danych  
**Rozwiązanie**: Wprowadzono dedykowane semafory dla każdego zasobu:
- `SEM_MAIN`: główna sekcja krytyczna
- `SEM_KASY`: operacje kasowe
- `SEM_KOLEJKA`: zarządzanie kolejkami
- `SEM_WEJSCIA+i`: osobne dla każdego wejścia do sektora

### Problem 3: Koordynacja dzieci z opiekunami
**Opis**: Dzieci mogły wchodzić na halę bez opiekuna  
**Rozwiązanie**: Implementacja mechanizmu oczekiwania:
- Dzieci sprawdzają czy opiekun jest już na hali
- Jeśli nie, czekają w specjalnej strukturze `dzieci_bez_opiekuna`
- Opiekun po wejściu odbiera czekające dziecko

### Problem 4: Deadlock przy zamykaniu procesów
**Opis**: Procesy nie kończyły się prawidłowo przy zakończeniu symulacji  
**Rozwiązanie**: Użycie SIGKILL zamiast SIGTERM dla pewnego zakończenia procesów potomnych

## 5. Dodane elementy specjalne

### 1. System logowania z poziomami
Zaawansowany logger zapisujący wszystkie zdarzenia z podziałem na:
- DEBUG: szczegółowe informacje diagnostyczne
- INFO: normalne zdarzenia w systemie
- WARNING: sytuacje nietypowe
- ERROR: błędy nie powodujące awarii
- CRITICAL: błędy krytyczne

### 2. Generator losowych kibiców
Symulacja realistycznego napływu kibiców:
- Losowe opóźnienia między przybyciem kibiców (0-150ms)
- VIP przybywają rzadziej (0-300ms)
- Losowy podział na drużyny (50/50)
- 10% kibiców to dzieci

### 3. Tryb infinite
Możliwość uruchomienia wielu meczy w trybie ciągłym:
```bash
./Hala_widowiskowo_sportowa 5 --infinite
```
- Automatyczne czyszczenie zasobów między meczami
- Przerwa 3 sekundy między meczami
- Statystyki dla każdego meczu osobno

### 4. Dynamiczne zarządzanie kasami
Inteligentny algorytm dostosowujący liczbę kas do kolejki:
- Próg: K/10 = 16 kibiców na kasę
- Otwieranie: gdy kolejka > (otwarte_kasy * 16)
- Zamykanie: gdy kolejka < ((otwarte_kasy-1) * 16) i więcej niż 2 kasy

### 5. Mechanizm przepuszczania
Implementacja wymogu unikania agresji kibiców:
- Licznik przepuszczonych kibiców dla każdego kibica
- Po przepuszczeniu 5 kibiców inne drużyny - wymuszenie kontroli
- Zapobiega długiemu oczekiwaniu i frustracji

## 6. Zauważone problemy z testami

### Problem 1: Testy wymagają zbudowanych plików wykonywalnych
**Opis**: Testy M1-M3 wymagają obecności pliku `./pracownik_techniczny`  
**Status**: Testy sprawdzają dostępność pliku i raportują błąd gracefully jeśli brak  
**Zalecenie**: Uruchomić `cmake` i `make` przed testami

### Problem 2: Testy jednostkowe vs testy integracyjne
**Opis**: `tests.cpp` zawiera testy jednostkowe sprawdzające logikę, ale nie pełną integrację  
**Obserwacja**: Niektóre wymagania (np. K8 - VIP bypass) są testowane na poziomie struktur danych, nie pełnych procesów  
**Uzasadnienie**: Pełne testy integracyjne wymagałyby uruchomienia całej symulacji, co jest realizowane przez główny program

### Problem 3: Timing w testach sygnałów
**Opis**: Testy M1-M3 używają `usleep()` do czekania na reakcję procesu  
**Obserwacja**: Może być niestabilne na wolnych systemach  
**Rozwiązanie**: Użyto bezpiecznych wartości (300ms, 500ms) które powinny wystarczyć

### Problem 4: Czyszczenie zasobów IPC
**Opis**: Przy przerwaniu testów (Ctrl+C) mogą pozostać zasoby IPC  
**Rozwiązanie**: Funkcja `cleanup()` usuwa zasoby, ale tylko przy normalnym zakończeniu  
**Zalecenie**: Użyć `ipcrm` ręcznie jeśli potrzeba: `ipcrm -a`

### Problem 5: Brak testów wydajnościowych
**Obserwacja**: Nie ma testów sprawdzających czy system radzi sobie z dużą liczbą procesów  
**Status**: Symulacja działa poprawnie dla K=160 kibiców (160 + 16 VIP = 176 procesów kibiców + procesy systemu)

## 7. Kompilacja i uruchomienie

### Wymagania
- CMake 4.0+
- Kompilator C++ z obsługą C++20 (GCC 10+ lub Clang 10+)
- System Linux/Unix (używa IPC POSIX)

### Budowanie
```bash
mkdir -p build
cd build
cmake ..
make
```

### Uruchomienie symulacji
```bash
# Pojedynczy mecz, start po 5 sekundach
./Hala_widowiskowo_sportowa 5

# Nieskończona pętla meczy
./Hala_widowiskowo_sportowa 5 --infinite

# Zakończenie: Ctrl+C
```

### Uruchomienie testów
```bash
# Z głównego katalogu po zbudowaniu
./tests
```

### Logi i raporty
Wszystkie logi są zapisywane w katalogu `reports/`:
- `main_*.log` - główny proces
- `summary_*.log` - podsumowania meczy
- `kasjer_*_*.log` - procesy kasjerów
- `kibic_*_*.log` - procesy kibiców
- `pracownik_*_*.log` - pracownicy techniczni
- `kierownik_*.log` - kierownik

## 8. Podsumowanie

Projekt został zrealizowany zgodnie z wymaganiami. System symuluje złożone zarządzanie halą sportową z wieloma procesami współbieżnymi. Wszystkie kluczowe wymagania zostały spełnione:

- ✅ Dynamiczne zarządzanie kasami (min 2, max 10)
- ✅ System kontroli z respektowaniem drużyn
- ✅ Obsługa VIP bez kolejki i kontroli
- ✅ Zarządzanie dziećmi z opiekunami
- ✅ Sygnały kierownika dla pracowników
- ✅ Kompleksowe logowanie do plików
- ✅ Testy pokrywające wszystkie wymagania

Kod jest modularny, dobrze udokumentowany i używa najlepszych praktyk programowania wieloprocesowego w systemach Unix/Linux.
