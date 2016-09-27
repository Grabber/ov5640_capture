#!/bin/bash

widht=(2592 2048 1920 1600 1280 1280 1024 800 640 320)
heigth=(1936 1536 1080 1200 960 720 768 600 480 240)

function reload() {
	modprobe -r -v vfe_v4l2
	modprobe -r -v ov5640
	modprobe ov5640 frame_rate="$1"
	modprobe vfe_v4l2
}

function load() {
	modprobe vfe_v4l2
	modprobe ov5640 frame_rate
}

function unload() {
	modprobe -r -v vfe_v4l2
	modprobe -r -v ov5640
}

for index in ${!widht[*]}
do
	reload "$1"
	echo "TEST $index"
	echo "Widht : ${widht[$index]}, Heigth : ${heigth[$index]}"
	sudo ./cap "${widht[$index]}" "${heigth[$index]}"
done