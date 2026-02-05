echo "--- Test Spojnosci Start ---"

mkdir -p /tmp
cd ..
cd cmake-build-debug || exit 1
LOG_FILE="tmp/test_spojnosci.log"


./Hala_widowiskowo_sportowa 2 20 2>&1 | tee "../tests/$LOG_FILE"
cd ../tests || exit 1

echo ""
echo "--- Analiza ---"

SPRZEDANE=$(grep "Sprzedane:" "$LOG_FILE" | tail -1 | awk -F'Sprzedane: ' '{print $2}' | awk -F'/' '{print $1}')
NA_HALI=$(grep "Na hali:" "$LOG_FILE" | tail -1 | awk -F'Na hali: ' '{print $2}')

echo "Sprzedane bilety: $SPRZEDANE"
echo "Kibice na hali: $NA_HALI"

DIFF=$((SPRZEDANE - NA_HALI))
if [ $DIFF -lt 0 ]; then
    DIFF=$((-DIFF))
fi

echo "Różnica: $DIFF"

# Wykryj błędy
SEGFAULT=$(grep -c "Segmentation fault\|core dumped" "$LOG_FILE" 2>/dev/null)
CORRUPTION=$(grep -c "corruption\|inconsistent\|mismatch" "$LOG_FILE" 2>/dev/null)

echo "Segfault: $SEGFAULT"
echo "Korupcja danych: $CORRUPTION"

echo ""
if [ "$SEGFAULT" -eq 0 ] && [ "$CORRUPTION" -eq 0 ] && [ "$DIFF" -eq 0 ]; then
    echo -e "\033[0;32m✓ Test PASSED - dane spójne\033[0m"
else
    echo -e "\033[0;31m✗ Test FAILED - wykryto problemy\033[0m"
fi

echo "--- Test Spojnosci End ---"