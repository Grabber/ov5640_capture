#!/bin/bash

gcc cap.c -o cap $(pkg-config --libs --cflags opencv) -lm