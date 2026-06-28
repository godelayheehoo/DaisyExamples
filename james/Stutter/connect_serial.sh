#!/bin/bash

# Find matching ports
PORTS=(/dev/cu.usbmodem*)

# Check if any port actually exists (in bash, if glob fails, it returns the glob string itself)
if [ ! -e "${PORTS[0]}" ]; then
    echo "Error: No Daisy serial ports (/dev/cu.usbmodem*) found."
    exit 1
fi

NUM_PORTS=${#PORTS[@]}

if [ "$NUM_PORTS" -eq 1 ]; then
    PORT="${PORTS[0]}"
    echo "Connecting to: $PORT"
    exec screen "$PORT"
else
    echo "Multiple Daisy serial ports found:"
    for i in "${!PORTS[@]}"; do
        echo "  [$((i+1))] ${PORTS[$i]}"
    done
    
    echo -n "Select a port (1-$NUM_PORTS): "
    read CHOICE
    
    # Validate selection
    if [[ "$CHOICE" =~ ^[0-9]+$ ]] && [ "$CHOICE" -ge 1 ] && [ "$CHOICE" -le "$NUM_PORTS" ]; then
        PORT="${PORTS[$((CHOICE-1))]}"
        echo "Connecting to: $PORT"
        exec screen "$PORT"
    else
        echo "Error: Invalid selection."
        exit 1
    fi
fi
