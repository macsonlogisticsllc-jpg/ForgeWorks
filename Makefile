TARGET = forgeworks
OBJS = src/main_psp.o src/screens.o src/render.o src/world.o src/data.o src/gfx.o src/assets.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wno-missing-field-initializers
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpsppower -lm

BUILD_PRX = 1
PSP_FW_VERSION = 500

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Forgeworks
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
