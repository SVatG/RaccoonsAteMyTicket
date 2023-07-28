# Grab settings
include ../config.mk

# Base directory is one up
BASEDIR := ../

# Binary name, author name, desc
TARGET := evoke23
AUTHOR := SVatG, k2 and SunSpire
DESC := raccoons ate my ticket

# Working directories
BUILD := $(BASEDIR)/build
SOURCES := $(BASEDIR)/source \
		   $(BASEDIR)/source/VectorLibrary \
		   $(BASEDIR)/source/GraphicsLibrary \
		   $(BASEDIR)/source/Shaders \
		   $(BASEDIR)/source/Rocket
DATA := $(BASEDIR)/data
ASSETS := $(BASEDIR)/assets
ROMFS := $(BASEDIR)/romfs

# Libraries
LIBDIRS := $(DEVKITPRO)/libctru
LIBS := -lcitro3d -lctru -lm

# Vpath for out of tree build
VPATH := $(BUILD):$(SOURCES):$(DATA):$(ASSETS):$(ROMFS)

# Tools
CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
NM := arm-none-eabi-nm
LD := $(CC)
TEX := tex3ds

# Tool flags
ARCH := -g -march=armv6k -mtune=mpcore -mfloat-abi=hard -mword-relocations
CFLAGS := -Wno-missing-braces -O3 -std=c99 -fomit-frame-pointer -ffunction-sections -mfloat-abi=hard -ffast-math $(ARCH) -D__3DS__ -DABGR32Pixels \
		$(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(BUILD) -DCONF_PLAYER=$(COFIG_PLAYER_MODE) -DCONF_ROCKET_IP='"$(CONFIG_PC_IP)"'
ASFLAGS := 
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) \
	   $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# Input files
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/Shaders/*.v.pica)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
PNGFILES := $(foreach dir,$(ASSETS),$(notdir $(wildcard $(dir)/tex_*.png)))
FBXFILES := $(foreach dir,$(ASSETS),$(notdir $(wildcard $(dir)/obj_*.fbx)))

# Output files
OUTPUT := $(CURDIR)/$(TARGET)
OFILES := $(addsuffix .o,$(BINFILES)) \
	  	  $(PICAFILES:.v.pica=.shbin.o) \
	  	  $(CFILES:.c=.o)
ROMFILES_PNG := $(PNGFILES:.png=.bin)
FBX_SENTINEL = .fbx_rebuild_sentry

# Want GNU extended syntax
.SUFFIXES:

# create dir if doesn't exist yet
$(ROMFS)/:
	@mkdir -vp "$@"

# Base compilation / assembly build rules
%.o: %.c music.ogg
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) -c $< -o $@ 

# PNG-to-bin with tex3ds
%.bin: %.png $(ROMFS)/
	$(TEX) $< -o $(ROMFS)/$@

# flac-to-ogg / mp3-to-ogg / ogg-to-ogg with basic music converter
%.ogg: %.flac $(ROMFS)/
	./../tooling/convertmusic.sh $< $(ROMFS)/$@ $(BUILD)/music_meta.h

%.ogg: %.mp3 $(ROMFS)/
	./../tooling/convertmusic.sh $< $(ROMFS)/$@ $(BUILD)/music_meta.h

%.ogg: %.oggi $(ROMFS)/
	./../tooling/convertmusic.sh $< $(ROMFS)/$@ $(BUILD)/music_meta.h

# FBX-to-bin with ufbxconv and 
$(FBX_SENTINEL): $(FBXFILES)
	@for fbx in $^; do \
		mkdir -vp $(ROMFS)/; \
		../tooling/ufbxconv $$fbx $(ROMFS)/; \
	done
	@touch $@

# Basic "binary blob" build rule
%.bin.o: %.bin
	bin2s $< | $(AS) -o $(@)
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(<F) | tr . _)`.h
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(<F) | tr . _)`.h
	echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(<F) | tr . _)`.h

# GPU shader assembly.
define shader-as
	$(eval CURBIN := $(patsubst %.shbin.o,%.shbin,$(notdir $@)))
	picasso -o $(CURBIN) $1
	bin2s $(CURBIN) | $(AS) -o $@
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u32" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(CURBIN) | tr . _)`.h
endef

%.shbin.o : %.v.pica %.g.pica
	$(call shader-as,$^)

%.shbin.o : %.v.pica
	$(call shader-as,$<)

# App metainfo
%.smdh: $(APP_ICON) $(MAKEFILE_LIST)
	smdhtool --create "$(TARGET)" "$(DESC)" "$(AUTHOR)" $(BASEDIR)/icon.png $@

# Binary from elf build rule
%.3dsx: %.elf $(ROMFS)/
	3dsxtool $< --smdh=$(BASEDIR)build/$(TARGET).smdh --romfs=$(ROMFS)/ $(_3DSXFLAGS) $@

# Linking rule
%.elf: 
	$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	$(NM) -CSn $@ > $(notdir $*.lst)

# Actual binary
all: $(OUTPUT).3dsx
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh $(ROMFILES_PNG) $(FBX_SENTINEL) music.ogg
$(OUTPUT).elf: $(OFILES)

clean:
	rm -fr $(TARGET).3dsx $(OUTPUT).smdh $(FBX_SENTINEL) $(TARGET).elf *.o *.h *.lst *.shbin *~ $(ROMFS)/tex_*.bin $(ROMFS)/music.ogg $(ROMFS)/*.vbo
	find . -type f -name "*:Zone.Identifier" -exec rm -f "{}" \;
