include config.mk

3DS_IP := $(CONFIG_3DS_IP)

all: upload

clean:
	$(MAKE) -C build clean
	rm tooling/ufbxconv

tooling/ufbxconv:
	$(MAKE) -C tooling

binary: tooling/ufbxconv
	$(MAKE) -C build all
	cp build/*.3dsx .

upload: binary
	3dslink -a $(3DS_IP) revision23.3dsx

test: binary
	cp revision23.3dsx $(CONFIG_CITRA_TEMP_FILE_TO)
	$(CONFIG_CITRA_COMMAND) $(CONFIG_CITRA_TEMP_FILE_FROM)