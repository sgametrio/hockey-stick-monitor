#!/bin/sh

#### DEFINITIONS

APPNAME=${PWD##*/}  # app name as current dir
PORT=$1
BOSSAC_PATH="/Users/luca/Library/Arduino15/packages/arduino/tools/bossac/1.9.1-arduino1/bossac"
TARGET="NRF52840_DK"
TOOLCHAIN="GCC_ARM"

##### COMMANDS

if [ "$#" -ne 1 ]; then
    echo "Please specify a port: ../compile-and-deploy.sh cu.usbmodem14101"
    exit 1
fi


mbed compile
arm-none-eabi-objcopy -O binary ./BUILD/${TARGET}/${TOOLCHAIN}/${APPNAME}_application.elf ./BUILD/${TARGET}/${TOOLCHAIN}/${APPNAME}.bin
${BOSSAC_PATH} -d --port=${PORT} -U -i -e -w ./BUILD/${TARGET}/${TOOLCHAIN}/${APPNAME}.bin -R
