CPPFLAGS += -Iinclude

all: ${BUILD}/lib${TARGET}.a

${BUILD}/lib${TARGET}.a: $(OBJECTS)
	@mkdir -p ${BUILD}
	${AR} rcs $@ $^

clean:
	rm -rf ${BUILD}
