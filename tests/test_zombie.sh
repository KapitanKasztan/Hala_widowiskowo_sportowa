echo "--- Test Zombie Start ---"

mkdir -p tmp
BEFORE_FILE="tmp/zombie_before.log"
AFTER_FILE="tmp/zombie_after.log"

./tests_src/zombie.sh > "$BEFORE_FILE"

cd ..
cd cmake-build-debug

./Hala_widowiskowo_sportowa 5 20
cd ..
cd tests

./tests_src/zombie.sh > "$AFTER_FILE"
if diff "$BEFORE_FILE" "$AFTER_FILE" >/dev/null; then
printf '%s\033[0;32m%s\033[0m\n' "Nie ma zombie - " "Test Passed"
else
    diff "$BEFORE_FILE" "$AFTER_FILE"
fi
echo "--- Zombie Test End ---"

