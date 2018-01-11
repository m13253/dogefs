.PHONY: all clean

all:
	$(MAKE) -C mkfs.dogefs $@
	$(MAKE) -C mount.dogefs $@

clean:
	$(MAKE) -C mkfs.dogefs $@
	$(MAKE) -C mount.dogefs $@
