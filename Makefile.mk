.PHONY: build deploy clean flash debug

all: build

build:
	west build -b chester_nrf52840

deploy:
	west build -b chester_nrf52840 -- -DOVERLAY_CONFIG=../chester/prj.conf.deploy

clean:
	west build -t pristine

flash:
	west flash

debug:
	west debug
