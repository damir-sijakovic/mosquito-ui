#!/bin/sh
# build.sh - compile the mosquito demo.
# Usage: ./build.sh           (uses SDL2_ttf for crisp text)
#        ./build.sh nottf     (no SDL2_ttf dependency, bitmap fallback)
set -e
if [ "$1" = "nottf" ]; then
  gcc -Wall -Wextra -O2 -DMSQ_NO_TTF demo.c -o demo $(sdl2-config --cflags --libs) -lm
  gcc -Wall -Wextra -O2 -DMSQ_NO_TTF demo_list.c -o demo_list $(sdl2-config --cflags --libs) -lm
  gcc -Wall -Wextra -O2 -DMSQ_NO_TTF child-windows-demo.c -o child_windows_demo $(sdl2-config --cflags --libs) -lm
else
  gcc -Wall -Wextra -O2 demo.c -o demo $(sdl2-config --cflags --libs) -lSDL2_ttf -lm
  gcc -Wall -Wextra -O2 demo_list.c -o demo_list $(sdl2-config --cflags --libs) -lSDL2_ttf -lm
  gcc -Wall -Wextra -O2 child-windows-demo.c -o child_windows_demo $(sdl2-config --cflags --libs) -lSDL2_ttf -lm
fi
echo "built ./demo"
echo "built ./demo_list"
echo "built ./child_windows_demo"
