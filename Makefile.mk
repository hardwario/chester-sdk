.PHONY: ble build deploy clean flash debug

all: build

ble:
	west build -b chester_nrf52840 -- \
		-DOVERLAY_CONFIG=../chester/prj.conf.ble

build:
	west build -b chester_nrf52840

deploy:
	west build -b chester_nrf52840 -- \
		-DOVERLAY_CONFIG=../chester/prj.conf.ble \
		-DOVERLAY_CONFIG=../chester/prj.conf.deploy

clean:
	rm -rf build

flash:
	west flash

debug:
	west debug
