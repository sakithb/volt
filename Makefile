.PHONY: build watch

build:
	make -C build/
	rm -rf dist/
	mkdir -p dist/
	mv build/engine.js build/engine.wasm build/engine.data dist/.
	cp index.html dist/.
	cp -r editor/* dist/.

	@echo -ne "\nBuilt on "
	@date +"%D %R"
	@echo -e "Watching...\n"

serve:
	cd dist; python3 -m http.server 8080

watch:
	@while true; do \
		clear -x; \
		GNUMAKEFLAGS=--no-print-directory $(MAKE) build; \
		inotifywait -qre close_write .; \
	done
