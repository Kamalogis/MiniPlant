#!/bin/bash
byobu new-session -d

# Pane pertama
byobu send-keys "cd ~/WTP/MiniPlant/Data_Handler" C-m

# Split vertikal
byobu split-window -h

# Pane kedua
byobu send-keys -t 1 "cd ~/WTP/MiniPlant/Data_Handler" C-m
byobu send-keys -t 1 "python3 main.py"

# Attach
byobu attach

