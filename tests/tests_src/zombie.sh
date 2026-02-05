#!/bin/bash
echo "=== BEFORE ==="
ipcs -a | grep $(whoami)
echo ""
echo "Zombie processes:"
ps aux | grep defunct | grep -E "(kibic|kasjer|pracownik)"
echo ""