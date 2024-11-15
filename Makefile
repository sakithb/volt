.PHONY: build watch

build:
	make -C build/
	rm -rf dist/
	mkdir -p dist/
	cp build/engine.js build/engine.wasm build/engine.data dist/.
	cp index.html dist/.
	cp -r editor/* dist/.

serve:
	cd dist; python3 -m http.server 8080

watch:
	while true; do \
		$(MAKE) build; \
		inotifywait -qre close_write .; \
	done
