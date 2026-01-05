* Build `gcc -shared -fPIC bindshim.c -o bindshim.so -ldl`
* Configure wine to use env variable `LD_PRELOAD=/path/to/bindshim.so`
