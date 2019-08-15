OS = $(shell uname -s)
MACHINE = $(shell uname -m)
BUILD = $(OS)_$(MACHINE)
TARGET=graph
SRCS = graph.cpp

cflags= -g -I . -I /usr/include -I../libs\
		-lfltk -L ../libs/$(BUILD)  -lpthread -lm -lfftw3 -lutils -L $(BUILD)
$(TARGET): $(SRCS)
	g++ -o $@ $^ $(cflags)

clean:
	rm $(TARGET)
