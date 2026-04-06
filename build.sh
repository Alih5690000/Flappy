emcc main.c -o index.html \
-s USE_SDL=2 \
-s USE_SDL_IMAGE=2 \
-s USE_SDL_MIXER=2 \
-s USE_SDL_TTF=2 \
-s SDL2_IMAGE_FORMATS='["png","jpg"]' \
-s SDL2_MIXER_FORMATS='["mp3","ogg"]' \
--preload-file assets@assets \
-O2 && python -m http.server 8000