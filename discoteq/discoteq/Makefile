.PHONY: all
all: flutter web-release linux

.PHONY:flutter
flutter:
	flutter config --no-analytics
	flutter config --enable-linux-desktop
	flutter pub get

build/web/index.html: ./html_src/index.html
	html-minifier --collapse-whitespace --remove-comments --remove-optional-tags --remove-redundant-attributes --remove-script-type-attributes --remove-tag-whitespace --use-short-doctype --minify-css true --minify-js true ./html_src/index.html > ./build/web/index.html

build/web/flutter_service_worker.js: html_src/flutter_service_worker.js
	terser -c -m -- ./html_src/flutter_service_worker.js > ./build/web/flutter_service_worker.js

web-static: build/web/index.html build/web/flutter_service_worker.js

.PHONY: web-release
web-release: web web-static
	#terser -c -m -- build/web/main.dart.js > ./build/web/main.dart.min.js
	#uglifyjs < ./build/web/main.dart.js  > ./build/web/main.dart.min.js
	#mv ./build/web/main.dart.min.js ./build/web/main.dart.js
	terser -c -- ./html_src/app.js > ./build/web/app.js

	
.PHONY: web
web:
	flutter build web --web-renderer html --profile --target lib/web.dart --csp
	#flutter build web --profile --target lib/web.dart
.PHONY: linux
linux:
	flutter build linux
	cp ./linux/README.txt ./build/linux/x64/release/bundle/.
	cd ./build/linux/x64/release/ && cp -r bundle discoteq && zip discoteq_desktop.zip discoteq/ -r && mv discoteq_desktop.zip ../../../../.


