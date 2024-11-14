.PHONY: build watch

build:
	make -C build/
	rm -rf dist/
	mkdir -p dist/
	mv build/engine.html build/engine.js build/engine.wasm dist/.
	mv dist/engine.html dist/index.html
	cp -r editor/* dist/.

serve:
	cd dist; python3 -m http.server 8080

watch:
	while true; do \
		$(MAKE) build; \
		inotifywait -qre close_write .; \
	done
