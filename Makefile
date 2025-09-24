.PHONY: build configure

configure:
	git submodule update --init --recursive
	meson setup build .

build:
	cd build && \
	meson compile && \
	meson install
	mavlink-routerd --version