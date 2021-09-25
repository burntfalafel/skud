CC = clang
CFLAGS = -O3 -Wall

TARGET = skud
TEST_TARGET1 = prog
TEST_TARGET2 = prog2
CFILES = include/skud.c include/util.c
TEST1_CFILES = tests/prog.c
TEST2_CFILES = tests/prog2.c

all: $(TARGET) $(TEST_TARGET1) $(TEST_TARGET2)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(CFILES)

$(TEST_TARGET1):
	$(CC) $(CFLAGS) -o $(TEST_TARGET1) $(TEST1_CFILES)

$(TEST_TARGET2):
	$(CC) $(CFLAGS) -o $(TEST_TARGET2) $(TEST2_CFILES)

clean:
	$(RM) $(TARGET) $(TEST_TARGET1) $(TEST_TARGET2)
