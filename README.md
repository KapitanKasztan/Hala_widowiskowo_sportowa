Temat 18 - Hala widowiskowo-sportowa.
W hali widowiskowo-sportowej o pojemności K kibiców ma zostać rozegrany mecz finałowy
siatkówki.. Kibice mogą być rozmieszczeni w 8 sektorach, każdy o pojemności K/8. Dodatkowo na
hali znajduje się sektor VIP.
Zakup biletów odbywa się bezpośrednio przed zawodami. W hali jest łącznie 10 kas. Zasady ich
działania przyjęte przez kierownika obiektu są następujące:
• Zawsze działają min. 2 stanowiska kasowe.
• Na każdych K/10 kibiców znajdujących się w kolejce do kasy powinno przypadać min. 1
czynne stanowisko kasowe.
• Jeśli liczba kibiców w kolejce jest mniejsza niż (K/10)*(N-1), gdzie N oznacza liczbę czynnych
kas, to jedna z kas zostaje zamknięta.
• Jeden kibic może kupić maksymalnie 2 bilety w tym samym sektorze;
• Bilety na miejsca w poszczególnych sektorach sprzedawane są losowo przez wszystkie kasy;
21
• Kasy są automatycznie zamykane po sprzedaży wszystkich miejscówek;
• Osoby VIP kupują bilet omijając kolejkę (mniej niż 0,3% * K).
Kibice przychodzą do kas w losowych momentach czasu (nawet po rozpoczęciu spotkania). Mecz
rozpoczyna się o godzinie Tp.
Z uwagi na rangę imprezy ustalono następujące rygorystyczne zasady bezpieczeństwa.
• Do każdego z 8 sektorów jest osobne wejście.
• Wejście na halę możliwe będzie tylko po przejściu drobiazgowej kontroli, mającej zapobiec
wnoszeniu przedmiotów niebezpiecznych.
• Kontrola przy każdym wejściu jest przeprowadzana równolegle na 2 stanowiskach, na
każdym z nich mogą znajdować się równocześnie maksymalnie 3 osoby.
• Jeśli kontrolowana jest więcej niż 1 osoba równocześnie na stanowisku, to należy
zagwarantować by byli to kibice tej samej drużyny.
• Kibic oczekujący na kontrolę może przepuścić w kolejce maksymalnie 5 innych kibiców.
Dłuższe czekanie wywołuje jego frustrację i agresywne zachowanie, którego należy unikać
za wszelką cenę.
• Istniej niewielka liczba kibiców VIP (mniejsza niż 0,3% * K), którzy wchodzą (i wychodzą) na
halę osobnym wejściem i nie przechodzą kontroli bezpieczeństwa (muszą mieć bilet kupiony
w kasie).
• Dzieci poniżej 15 roku życia wchodzą na stadion pod opieką osoby dorosłej.
Po wydaniu przez kierownika polecenia (sygnał1) pracownik techniczny wstrzymuje wpuszczanie
kibiców do danego sektora. Po wydaniu przez kierownika polecenia (sygnał2) pracownik techniczny
wznawia wpuszczanie kibiców. Po wydaniu przez kierownika polecenia (sygnał3) wszyscy kibice
opuszczają stadion – w momencie gdy wszyscy kibice opuścili dany sektor, pracownik techniczny
wysyła informację do kierownika.
Napisz odpowiednie programy kierownika hali, kasjera, pracownika technicznego i kibica. Raport z
przebiegu symulacji zapisać w pliku (plikach) tekstowym.

Testy
# Tabela testów

## Wymaganie: Hala i sektory

| Nr | ID | Wymaganie | Nazwa testu | Cel | Przebieg testu | Dane testowe | Oczekiwany rezultat |
|----|----|-----------|-------------|-----|----------------|--------------|---------------------|
| 1 | H1 | Hala musi mieć pojemność K | Sprawdzenie Przepełnienia | Przetestować zachowanie podczas przepełnienia hali | Liczba kibiców K+n | Kibice | Zostaną wpuszczono K kibiców |

## Wymaganie: Kasy i zakup biletów

| Nr | ID | Wymaganie | Nazwa testu | Cel | Przebieg testu | Dane testowe | Oczekiwany rezultat |
|----|----|-----------|-------------|-----|----------------|--------------|---------------------|
| 2 | K2 | System musi zapewniać, że co najmniej 2 kasy są zawsze otwarte. Na każde K/10 kibiców w kolejce musi przypadać co najmniej 1 czynne stanowisko. | Sprawdzenie czy 2 kasy są zawsze otwarte | Sprawdzenie czy 2 kasy są zawsze otwarte | Sprawdźić ilość otwartych kas podczas maksymalnego obłożenia, i zerowego obłożenia | Oczekujący kibice, otwarte kasy | Zawsze co najmniej 2 kasy są otwarte |
| 3 | K3 | [j.w.] | Test otwierania kas | Przetestować zachowanie systemu kasowego w przyroście kibiców | Loop dodający kibiców | Oczekujący kibice, otwarte kasy | po przekroczeniu (otwarte kasy * K/10) osób w kolejce, otwiera się kolejna weryfikacja, czy po spadnięciu poniżej (K/10)*(N-1) kibiców, jedna kasa jest zamykana |
| 4 | K4 | Jeśli liczba osób w kolejce spada poniżej (K/10)*(N-1), jedna kasa jest zamykana | Test zamykania kas | Przetestować zachowanie systemu kasowego w zmniejszającej się kolejce kibiców | Loop odejmujący/przepuszczając kibiców | Oczekujący kibice, otwarte kasy | [j.w.] |
| 5 | K5 | Jeden kibic może kupić max 2 bilety w jednym sektorze. | Próba zakupu +2 biletów | Przetestować zachowanie systemu kasowego w przypadku eksploracji więcej niż 2 biletów | Próba reqstu więcej niż dwóch biletów przez kibica (2,3,4) | ilość biletów które kibic chce kupic | System blokuje możliwość zakupu +2 biletów |
| 6 | K7 | Kasy zamykają się automatycznie po wyczerpaniu biletów. | Zamykanie wszystkich kas | Przetestować czy kasy zamykają się przy braku biletów | Liczba dostępnych biletów = 0 | Liczba dostępnych biletów | Kasy zamykają się |
| 7 | K8 | Kibice VIP kupują bilety z pominięciem kolejki. | Kibic VIP bez kolejki | Przetestować czy kibic VIP wchodzi bez kolejki | Kibice VIP w kolejce > 0 | Kibice VIP w kolejce | Kibic pomija kolejkę |

## Wymagania dotyczące kontroli

| Nr | ID | Wymaganie | Nazwa testu | Cel | Przebieg testu | Dane testowe | Oczekiwany rezultat |
|----|----|-----------|-------------|-----|----------------|--------------|---------------------|
| 8 | C3 | Na stanowisku mogą być max 3 osoby jednocześnie. | Sprawdzenie limitu osób na stanowisku | Przetestować limit osób na stanowisku | osoby oczekujące na kontrole > 3 | osoby oczekujące na kontrole | tylko 3 osoby są brane do kontroli |
| 9 | C4 | Jeśli kontrolowanych jest więcej osób naraz, muszą być z tej samej drużyny. | Sprawdzenie poprawności kontroli | Przetestować czy do kontroli brani są kibice tylko jednej drużyny | Osoby oczekujące w kolejce są z Innych drużyn | Osoby oczekujące w kolejce | do kontroli brani są kibice tylko jednej drużyny |
| 10 | C5 | Kibic może przepuścić max 5 osób; więcej prowadzi do agresji (sytuacja krytyczna). | Limit przepuszczania | Przetestować czy kibic maksymalnie przepuścił 5 osób | Sytuacja gdy kibic musiałby przepuścić > 5 kibiców innej drużyny | | Po przepuszczeniu 5 kibiców, kibic jest kontrolowany |
| 11 | C6 | Dzieci < 15 lat wchodzą z dorosłym. | Test wejścia dzieci | Przetestować czy system będzie wymagał dorosłego przy dziecku < 15 | Wpuście dzieci w wieku 14, 15 bez opiekuna | dzieci w wieku 14, 15 | dziecko 15 jest wpuszczone, dziecko 14 nie |
| 12 | C7 | VIP nie przechodzą kontroli. | Wejście VIP-a | Przetestować czy vip nie przechodzi kontoli | wpuście vipa i sprawdzic czy przechodzi kontrole | vip | vip nie przechodzi kontroli |

## Wymagania dotyczące kierownika

| Nr | ID | Wymaganie | Nazwa testu | Cel | Przebieg testu | Dane testowe | Oczekiwany rezultat |
|----|----|-----------|-------------|-----|----------------|--------------|---------------------|
| 13 | M1 | sygnał1 ⇒ zatrzymanie wpuszczania. | Test sygnału 1 | Przetestować czy po puszczeniu sygnału 1, pracownik techniczny przestaje wpuszczać kibiców | Wprowadzic sygnał 1 podczas wpuszczania | wpuszczani kibice, sygnł 1 | wpuszczanie wstrzymane |
| 14 | M2 | sygnał2 ⇒ wznowienie wpuszczania. | Test sygnału 2 | Przetestować czy po puszczeniu sygnału 2, pracownik techniczny wznawia wpuszczanie | Wprowadzic sygnał 2 przy wstrzymanym wpuszczaniu | wstrzyamni kibice, sygnał 2 | wpuszczanie wznowione |
| 15 | M3 | sygnał3 ⇒ wszyscy kibice opuszczają stadion, po przejściu do sektoru sektor zgłasza „pusty". | Test sygnału 3 | Przetestować czy po puszczeniu sygnału 3, kibice opuszczają stadion, oraz przetestować czy po opuszczeniu, pracownik techniczny zgłasza 'pusty' | Wprowadzic sygnał 3 | Pełny sektor, sygnał 3 | Sektor się opróżnia, pracownik zgłasza "pusty" |
