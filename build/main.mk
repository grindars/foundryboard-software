ROOT   := $(abspath $(dir $(word 2,$(MAKEFILE_LIST)))/..)
SUBDIR := $(patsubst $(ROOT)/%,%,$(abspath $(dir $(firstword $(MAKEFILE_LIST)))))

BUILD := ${ROOT}/out/${SUBDIR}

include $(ROOT)/project.mk

OBJECTS := $(foreach name,$(basename ${SOURCES}),${BUILD}/$(name).o)
DEPENDS := $(foreach name,$(basename ${SOURCES}),${BUILD}/$(name).d)

${BUILD}/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -MD $(CFLAGS) -c -o $@ $<

${BUILD}/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -MD $(CFLAGS) -c -o $@ $<

include $(ROOT)/build/$(TEMPLATE).mk
-include ${DEPENDS}
