.PHONY: build

build:
	cd build && \
	meson compile && \
	meson install
	mavlink-routerd --version