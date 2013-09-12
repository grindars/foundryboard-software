TEMPLATE = none
TARGETS  = bootloader-ram
SOURCES  = main.c
LIBS   = ${ROOT}/out/libs/cmsis/libcmsis.a \
		 ${ROOT}/out/libs/libc/libc.a \
		 ${ROOT}/out/libs/dispatch/libdispatch.a

include ../build/main.mk

all: $(addprefix ${BUILD}/,${TARGETS})

clean:
	rm -rf ${BUILD}

${BUILD}/%: %.lds ${OBJECTS} ${LIBS}
	@mkdir -p $(dir $@)
	${LD} -o $@ ${LDFLAGS} -\( -T$^ -\)

${BUILD}/%.hex: ${BUILD}/%
	${OBJCOPY} -O ihex $< $@

run-%: %.gdb ${BUILD}/% 
	${GDB} --batch -x $^

debug-%: %.gdb ${BUILD}/%
	${GDB} -x $^

flash-%: ${BUILD}/%-flash.hex
	gdbflasher --mcu stm32l1xx $<
