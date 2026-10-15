# Makefile for takt1ccname (Custom MIDI CC Names mod for Elektron Digitakt I)

ROOT := $(shell pwd)
STOCK ?= $(HOME)/Downloads/Digitakt_OS1.54.syx
CROSS ?= $(ROOT)/toolchain/m68k-linux-musl-cross/bin/m68k-linux-musl-
PYTHON ?= $(ROOT)/digiemu/.venv/bin/python
VERSION ?= DCCN

all: mod

words.h: words.txt tools/gen_words.py
	@echo "Generating words.h from words.txt..."
	$(PYTHON) tools/gen_words.py words.txt words.h

mod: words.h
	@echo "Building takt1ccname .elemod..."
	@mkdir -p out
	ELEKLOADER_CROSS=$(CROSS) PYTHONPATH=$(ROOT)/elekloader $(PYTHON) -m elekloader.sdk.build $(ROOT) --stock $(STOCK) --out $(ROOT)/out

test: mod
	@echo "Running tests in Unicorn..."
	PYTHONPATH=$(ROOT)/elekloader $(PYTHON) tests/test_ccname.py

syx: mod
	@echo "Building combined patched firmware syx..."
	@mkdir -p out
	PYTHONPATH=$(ROOT)/elekloader $(PYTHON) -m elekloader.patch \
		--stock $(STOCK) \
		--mod $(ROOT)/elekloader/mods/core/out/core-3.0-os1.54.elemod \
		--mod $(ROOT)/out/takt1ccname-1.0-os1.54.elemod \
		--out $(ROOT)/out/Digitakt_OS1.54_ccname.syx \
		--version $(VERSION)
	@echo "Firmware created at: $(ROOT)/out/Digitakt_OS1.54_ccname.syx"

lint: mod
	@echo "Linting takt1ccname .elemod..."
	PYTHONPATH=$(ROOT)/elekloader $(PYTHON) -m elekloader.lint $(ROOT)/out/takt1ccname-1.0-os1.54.elemod

clean:
	rm -rf out *.work /tmp/Digitakt_OS1.54_ccname.syx words.h

.PHONY: all mod test lint syx clean
