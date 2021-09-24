CC = clang
CFLAGS = -O3 -Wall

TARGET = skud
CFILES = include/skud.c include/util.c

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(CFILES)

clean:
	$(RM) $(TARGET)
