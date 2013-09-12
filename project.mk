CROSS_COMPILE := ${HOME}/probe/tools/bin/arm-none-eabi-
CC            := ${CROSS_COMPILE}gcc
CXX           := ${CROSS_COMPILE}g++
AR            := ${CROSS_COMPILE}ar
LD            := ${CROSS_COMPILE}ld
OBJCOPY       := ${CROSS_COMPILE}objcopy
GDB           := ${CROSS_COMPILE}gdb

CFLAGS        := -O2 -Wall -W --std=c11 -mcpu=cortex-m3 -msoft-float -mthumb -g
CPPFLAGS      := -nostdinc -DSTM32L1XX_MD -I${ROOT}/libs/libc/include -I${ROOT}/libs/cmsis/include \
    -I${ROOT}/libs/dispatch/include
