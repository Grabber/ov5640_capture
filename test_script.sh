#!/bin/bash

widht=(2592 2048 1920 1600 1280 1280 1024 800 640 320)
heigth=(1936 1536 1080 1200 960 720 768 600 480 240)

function reload() {
	sleep .5
	rmmod vfe_v4l2
	rmmod ov5640
	sleep .5
	modprobe vfe_v4l2
	insmod ov5640_build/ov5640.ko frame_rate="$1"
	sleep .5
}

function load() {
	modprobe vfe_v4l2
	insmod ov5640_build/ov5640.ko
}

function unload() {
	rmmod vfe_v4l2
	rmmod ov5640
}

for index in ${!widht[*]}
do
    reload "$1"
    echo "TEST $index"
    echo "Widht : ${widht[$index]}, Heigth : ${heigth[$index]}"
    sudo ./capture "${widht[$index]}" "${heigth[$index]}"
done