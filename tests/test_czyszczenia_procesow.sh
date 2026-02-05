echo "--- Test Czyszczenia Procesow (po sygnale SIGINT) Start ---"

cd ..
cd cmake-build-debug
./Hala_widowiskowo_sportowa 2 60 &
MAIN_PID=$!
cd ..
cd tests
sleep 5

kill -SIGINT $MAIN_PID

wait $MAIN_PID

echo "-----------------------------------"

WISZACE=$(pgrep -x "kasjer|kibic|kierownik|pracownik_techniczny|kibic_vip")

if [ -z "$WISZACE" ]; then
    printf '%s\033[0;32m%s\033[0m\n' "Wszystkie procesy wyczyszczone - " "Test Passed"
else
    printf '%s\033[0;31m%s\033[0m\n' "Nie wszystkie procesy sie wyczyscily - " "Test Failed"
    echo "Pozostałe procesy:"
    echo "$WISZACE"
fi

echo "--- Test Czyszczenia Procesow (po sygnale SIGINT) End ---"