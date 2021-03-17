.PHONY: build clean flash

all: build

build:
	west build -b chester_nrf52840

clean:
	west build -t pristine

flash:
	west flash

debug:
	west debug
