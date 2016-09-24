#!/bin/bash

gcc `pkg-config --libs --cflags opencv` -lm cap.c -o cap