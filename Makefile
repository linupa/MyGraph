OS = $(shell uname -s)
MACHINE = $(shell uname -m)
BUILD = $(OS)_$(MACHINE)

EXECUTABLE = graph
SOURCES = graph.cpp
OBJECTS = $(patsubst %.cpp,$(BUILD)/%.o,$(SOURCES))
TARGET = $(BUILD)/$(EXECUTABLE)

cflags= -g -I . -I /usr/include
ldflags= -O3 -g -Wl,-L /usr/lib -lfltk -lm -lpthread

all: $(BUILD) $(EXECUTABLE) $(TARGET)

%.o: %.cpp
	g++ -c $^ $(cflags)

$(TARGET): ${OBJECTS}
	g++ -o $@ $^ $(ldflags)

$(OBJECTS): $(BUILD)/%.o : %.cpp
	g++ -c $< $(cflags) -o $@

$(BUILD):
	mkdir $(BUILD)

clean:
	rm -f $(TARGET) $(OBJECTS)

$(EXECUTABLE):
	ln -sf $(TARGET) $(EXECUTABLE)

