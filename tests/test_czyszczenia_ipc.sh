echo "--- Test Czyszczenia IPC Start ---"

mkdir -p tmp
BEFORE_FILE=tmp/czyszczenie_ipc_before.log
AFTER_FILE=tmp/czyszczenie_ipc_after.log

echo "Przed: "
ipcs -m -s -q | grep "$(whoami)" | tee "$BEFORE_FILE" | wc -l

cd ..
cd cmake-build-debug
./Hala_widowiskowo_sportowa 5 20
cd ..
cd tests

sleep 2
echo ""
echo "Po: "
ipcs -m -s -q | grep "$(whoami)" | tee "$AFTER_FILE" | wc -l
POZOSTALO=$(wc -l < "$AFTER_FILE")
echo "Pozostało zasobów IPC: $POZOSTALO"

if diff -q "$BEFORE_FILE" "$AFTER_FILE" >/dev/null 2>&1; then
    printf '%s\033[0;32m%s\033[0m\n' "Wszystkie zasoby IPC wyczyszczone - " "Test Passed"
else
    printf '%s\033[0;31m%s\033[0m\n' "Pozostało $POZOSTALO zasobów IPC - " "Test Failed"
    echo "Szczegóły (po):"
    cat "$AFTER_FILE"
    echo ""
    echo "Różnice względem stanu przed (diff):"
    diff -u "$BEFORE_FILE" "$AFTER_FILE" || true
fi

echo "--- Test Czyszczenia IPC End ---"
