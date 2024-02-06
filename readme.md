I compiled this with clang on Apple Silicon. If you aren't on that your mileage may vary.

```
clang snake.c -o build/snake
```

Also compiled to wasm using emscripten:

```
emcc snake.c -o build/web/snake.html
```
