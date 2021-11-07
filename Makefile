FORMAT_FILES=$(shell find CO2-Ampel_Plus -iname '*.h' -print0 -or -iname '*.cpp' -print0 -or -iname '*.ino' -print0 | xargs -0 echo)
FORMAT_COMMAND="--style=Chromium -verbose -i $(FORMAT_FILES)"
FORMAT_TEST_COMMAND="--style=Chromium -verbose --Werror --dry-run $(FORMAT_FILES)"
CO2AMPEL_DEV=/dev/ttyACM0

.PHONY: clean local-env build-builder build-builder-formatter build format format-test upload build-upload configure get-configure

clean:
	@docker container rm co2ampel-builder
	@cd build; rm -rf $(shell cd build; find . -iname "*" ! -iwholename "./.keep" ! -iwholename ".")

local-env:
	curl -o arduino-builder/arduino-cli-config.py https://raw.githubusercontent.com/washed/arduino-builder/0.1.2/arduino-cli-config.py
	curl -o arduino-builder/requirements.txt https://raw.githubusercontent.com/washed/arduino-builder/0.1.2/requirements.txt
	pip install -r arduino-builder/requirements.txt
	python arduino-builder/arduino-cli-config.py --no-compile --arduino-cli /usr/bin/arduino-cli

build-builder:
	docker-compose build arduino-builder-co2ampel

build-builder-formatter:
	docker-compose build arduino-builder-co2ampel-format

build: build-builder
	docker-compose up arduino-builder-co2ampel
	docker cp co2ampel-builder:/usr/src/app/build ./

format: build-builder-formatter
	FORMAT_COMMAND=$(FORMAT_COMMAND) USER="$(shell id -u):$(shell id -g)" docker-compose up arduino-builder-co2ampel-format

format-test: build-builder-formatter
	FORMAT_COMMAND=$(FORMAT_TEST_COMMAND)  USER="$(shell id -u):$(shell id -g)" docker-compose up arduino-builder-co2ampel-format

upload:
	arduino-cli upload CO2-Ampel_Plus -b co2ampel:samd:sb --input-dir build -p $(CO2AMPEL_DEV)

build-upload: build upload

configure:
	echo -ne 'set buzzer 0;' > $(CO2AMPEL_DEV)
	echo -ne 'set wifi_password ${WIFI_PASSWORD};' > $(CO2AMPEL_DEV)
	echo -ne 'set wifi_ssid ${WIFI_SSID};' > $(CO2AMPEL_DEV)

get-configure:
	echo -ne 'get buzzer;' > $(CO2AMPEL_DEV)
	echo -ne 'get wifi_ssid;' > $(CO2AMPEL_DEV)
	echo -ne 'get wifi_password;' > $(CO2AMPEL_DEV)
