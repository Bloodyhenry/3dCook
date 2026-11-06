#-------------------------------------------------------------------------------
# Appli 3DS « Recettes mains sales »
#-------------------------------------------------------------------------------
# Cibles :
#   make            compile recettes.3dsx (nécessite devkitPro + devkitARM)
#   make envoyer    envoie le .3dsx à la console par Wi-Fi (3dslink)
#                   ex. : make envoyer IP=192.168.1.42
#   make cia        fabrique recettes.cia (installation dans le menu HOME)
#                   nécessite makerom et bannertool (voir README.md, section CIA)
#   make installer-cia IP=…  installe recettes.cia par Wi-Fi avec FBI
#   make test       tests des modules portables, compilés pour le Mac (clang)
#   make clean      supprime les fichiers compilés
#
# Basé sur le modèle officiel des exemples devkitPro (3ds-examples).
#-------------------------------------------------------------------------------
.SUFFIXES:

# Tests sur le Mac : ne demandent pas devkitPro.
ifeq ($(MAKECMDGOALS),test)
test:
	@$(MAKE) --no-print-directory -C tests
else

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM n'est pas défini : installer devkitPro (voir README.md)")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#-------------------------------------------------------------------------------
# TARGET   nom du fichier produit
# SOURCES  dossiers des sources C
# INCLUDES dossiers d'en-têtes
#-------------------------------------------------------------------------------
TARGET      := recettes
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := source
GRAPHICS    :=
ROMFS       :=

APP_TITLE       := Recettes
APP_DESCRIPTION := Recettes sans les mains
APP_AUTHOR      := jibz

#-------------------------------------------------------------------------------
# Options de compilation
#-------------------------------------------------------------------------------
ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS   := -g -Wall -Wextra -O2 -mword-relocations \
            -ffunction-sections \
            $(ARCH)
CFLAGS   += $(INCLUDE) -D__3DS__ -std=gnu11

ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS     := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS  := $(CTRULIB)

#-------------------------------------------------------------------------------
# Rien à modifier en dessous (mécanique standard de devkitPro)
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES  := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD := $(CC)
export OFILES_SOURCES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES         := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# Icône : icon.png à la racine si présente, sinon celle de libctru.
ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring icon.png,$(icons)))
		export APP_ICON := $(TOPDIR)/icon.png
	else
		export APP_ICON := $(CTRULIB)/default_icon.png
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean envoyer cia installer-cia

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo nettoyage...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia
	@$(MAKE) --no-print-directory -C tests clean

# 3dslink : lancer d'abord le Homebrew Launcher sur la console et appuyer sur Y.
envoyer: all
	3dslink $(if $(IP),-a $(IP)) $(TARGET).3dsx

#-------------------------------------------------------------------------------
# Paquet CIA. Les outils sont cherchés dans outils/ (à côté de ce Makefile),
# puis dans le PATH. L'icône est celle du .3dsx (icon.png → recettes.smdh) ;
# la bannière et son son sont dans cia/ (cia/fabriquer_visuels.py les refait).
#-------------------------------------------------------------------------------
MAKEROM    ?= $(firstword $(wildcard $(CURDIR)/outils/makerom) makerom)
BANNERTOOL ?= $(firstword $(wildcard $(CURDIR)/outils/bannertool) bannertool)
VERSION    := 1 0 0

# Bannière seulement si bannertool est disponible (il n'existe pas de version
# macOS publiée : sans lui, le CIA est fait sans bannière).
AVEC_BANNIERE := $(shell command -v $(BANNERTOOL) >/dev/null 2>&1 && echo oui)

cia: all
	@command -v $(MAKEROM) >/dev/null 2>&1 || { echo "makerom introuvable : voir README.md, section « Paquet CIA »."; exit 1; }
	@mkdir -p $(BUILD)
ifeq ($(AVEC_BANNIERE),oui)
	$(BANNERTOOL) makebanner -i cia/banniere.png -a cia/banniere.wav -o $(BUILD)/banniere.bnr
else
	@echo "ATTENTION : bannertool introuvable, CIA sans bannière (l'icône reste affichée)."
endif
	$(MAKEROM) -f cia -o $(TARGET).cia -target t -exefslogo -rsf cia/recettes.rsf \
		-elf $(TARGET).elf -icon $(TARGET).smdh \
		$(if $(AVEC_BANNIERE),-banner $(BUILD)/banniere.bnr) \
		-major $(word 1,$(VERSION)) -minor $(word 2,$(VERSION)) -micro $(word 3,$(VERSION))
	@echo "$(TARGET).cia créé."

# Installe le .cia par Wi-Fi avec FBI (sur la console : FBI → Remote Install →
# Receive URLs over the network). Ex. : make installer-cia IP=192.168.1.67
installer-cia: cia
	@test -n "$(IP)" || { echo "Indiquer l'adresse de la console : make installer-cia IP=192.168.1.67"; exit 1; }
	python3 cia/envoyer_cia.py $(IP) $(TARGET).cia

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
$(OFILES_SOURCES) : $(HFILES)
$(OUTPUT).elf : $(OFILES)

%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
endif
