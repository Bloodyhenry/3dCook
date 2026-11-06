# Tests des modules portables de l'appli, compilés pour le Mac (ou Linux).
# Lancement : make test (depuis app3ds/) ou make (depuis tests/).
CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O1 -g -I../source -fsanitize=address,undefined
LDLIBS  := -lm

PORTABLES := $(filter-out ../source/main.c ../source/dessin.c ../source/vue_liste.c ../source/vue_recette.c ../source/reseau.c ../source/saisie.c ../source/alarme.c ../source/vue_minuteurs.c ../source/micro.c ../source/vue_reglages.c ../source/camera.c ../source/photo_ecran.c,$(wildcard ../source/*.c))
TESTS     := test_echelle test_mise_en_page test_json test_recette_json test_synchro test_minuterie test_souffle test_mouvement test_photo

.PHONY: all clean syntaxe
all: $(TESTS) syntaxe
	@for t in $(TESTS); do ./$$t || exit 1; done

# Vérifie la syntaxe et les types du code 3DS avec de FAUX en-têtes
# (stubs/) : ne remplace pas la vraie compilation avec devkitPro.
syntaxe:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Istubs -I../source \
		../source/dessin.c ../source/vue_recette.c ../source/vue_liste.c ../source/vue_minuteurs.c ../source/vue_reglages.c ../source/main.c
	@echo "syntaxe du code 3DS : OK (en-têtes factices)"

test_%: test_%.c $(PORTABLES) ../source/*.h verif.h
	$(CC) $(CFLAGS) -o $@ $< $(PORTABLES) $(LDLIBS)

clean:
	rm -rf $(TESTS) verifier_dossier synchro_hote *.dSYM

# Outil (hors make test) : lit tout un dossier de recettes avec le code de l'appli.
verifier_dossier: verifier_dossier.c $(PORTABLES) ../source/*.h
	$(CC) $(CFLAGS) -o $@ $< $(PORTABLES) $(LDLIBS)

# Outil (hors make test) : synchronisation réelle contre une passerelle, via curl.
synchro_hote: synchro_hote.c $(PORTABLES) ../source/*.h
	$(CC) $(CFLAGS) -o $@ $< $(PORTABLES) $(LDLIBS)
