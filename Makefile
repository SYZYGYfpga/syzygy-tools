
INCLUDEDIR = include

CFLAGS += -Wall

TEST_BLOBS = test-dna-blobs/pod-tst-doublewide.bin test-dna-blobs/pod-pmod.bin

all: smartvio-test dna-writer


smartvio-test: smartvio/smartvio-test.cpp smartvio/syzygy.o
	$(CXX) $(CFLAGS) -std=c++11 -I $(INCLUDEDIR) -o $@ $^


dna-writer: dna-tools/dna-writer.cpp smartvio/syzygy.o
	$(CXX) $(CFLAGS) -std=c++11 -I $(INCLUDEDIR) -Ismartvio -o $@ $^


smartvio/syzygy.o: smartvio/syzygy.c
	$(CC) $(CFLAGS) -I $(INCLUDEDIR) -o $@ -c $^


$(TEST_BLOBS): test-dna-blobs/%.bin: pod-dna/%.json
	mkdir -p test-dna-blobs
	./dna-writer $< $@ ABC


.PHONY: clean runtests

runtests: dna-writer smartvio-test $(TEST_BLOBS)
	./smartvio-test
	

clean:
	rm -f smartvio-test dna-writer smartvio/syzygy.o
	rm -rf test-dna-blobs
