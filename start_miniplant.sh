#!/bin/bash

# Cek apakah sesi "miniplant" sudah ada.
# Jika sudah ada, skrip akan keluar.
if byobu has-session -t miniplant; then
    echo "Sesi 'miniplant' sudah ada, tidak perlu membuat yang baru."
    exit 0
fi

# Jika sesi tidak ada, buat sesi baru
byobu new-session -d -s "miniplant"

# Pane pertama
byobu send-keys -t "miniplant":0.0 "cd ~/WTP/MiniPlant/Data_Handler" C-m

# Split vertikal
byobu split-window -h -t "miniplant":0.0

# Pane kedua
byobu send-keys -t "miniplant":0.1 "cd ~/WTP/MiniPlant/Data_Handler" C-m
byobu send-keys -t "miniplant":0.1 "python3 main.py" C-m

exit 0
