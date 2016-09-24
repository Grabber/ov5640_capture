#!/bin/bash

gcc cap.c -o cap $(pkg-config --libs --cflags opencv) -lm
gcc cap_rgb.c -o cap_rgb $(pkg-config --libs --cflags opencv) -lm