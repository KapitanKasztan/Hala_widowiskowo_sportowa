echo "--- Test Zombie Start ---"

mkdir -p tmp
BEFORE_FILE="tmp/zombie_before.log"
AFTER_FILE="tmp/zombie_after.log"

# Zapisz tylko zombie przed uruchomieniem
ps aux | awk '$8 ~ /Z/' > "$BEFORE_FILE"

cd ..
cd cmake-build-debug

./Hala_widowiskowo_sportowa 5 20
cd ..
cd tests

# Zapisz tylko zombie po uruchomieniu
ps aux | awk '$8 ~ /Z/' > "$AFTER_FILE"

# Sprawdz czy pojawiły się nowe zombie
if diff "$BEFORE_FILE" "$AFTER_FILE" >/dev/null; then
    printf '%s\033[0;32m%s\033[0m\n' "Nie ma zombie - " "Test Passed"
else
    printf '\033[0;31m%s\033[0m\n' "Wykryto zombie!"
    diff "$BEFORE_FILE" "$AFTER_FILE"
fi

echo "--- Zombie Test End ---"