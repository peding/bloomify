# bloomify

A weird stegnography tool using .gnu.hash section in ELF binary.  

Convert arbitrary file into a suspicious c file that contains only tons of suspicious variables.  
The original data will magically appear inside binary when you compile it as a shared library.

Don't use this shit on large files, recommended for files < 128 KiB.

## Demo

If you check the ```joy64.c```, you will see that it declares a lot of int variables and nothing else:
```c
int cJh7ox593LtjRYDL;
int Mr8maQMFTUWCZza2;
int jhYkEQiurzE6k7HD;
int Ey4Yd_gsyKoJET_Q;
 ... ... ... ... ...
int sVXczBNYjtLVtWQu;
int yux1V33qPYsTFDca;
int r_k73alFZDsC4nlR;
int HckQ9Sm5luhhRGlk;
```

Try out compiling this c file (no need to execute) generated using this tool, use ```joy32.c``` if you compile for 32 bit machine.
```bash
gcc -o joy.so joy64.c -shared
```

Then check the file content using ```cat``` command
```bash
cat joy.so | less
```

You should be able to see this ascii art inside the binary:
```
⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⠋⣉⣉⣉⣉⣉⣉⠙⠛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿ 
⣿⣿⣿⣿⣿⡿⠟⢁⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⡈⠻⢿⣿⣿⣿⣿⣿ 
⣿⣿⣿⡿⠋⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠙⢿⣿⣿⣿ 
⣿⣿⡟⢀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⢻⣿⣿ 
⣿⡟⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⢻⣿ 
⣿⢀⣿⣿⣿⠟⠁⣠⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣄⠈⠻⣿⣿⣿⡀⣿ 
⡇⢸⣿⣿⠋⣠⡾⠿⠛⠛⠛⠿⣿⣿⣿⣿⣿⣿⠿⠛⠛⠛⠻⢷⣄⠙⣿⣿⡇⢸ 
⡇⢸⣿⣿⣾⣿⢀⣠⣤⣤⣤⣤⣀⣿⣿⣿⣿⣀⣤⣤⣤⣤⣄⡀⣿⣷⣾⣿⡇⢸ 
⡇⠸⠟⣫⣥⣶⣧⠹⠿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠏⣼⣶⣬⣍⠻⠇⢸ 
⡧⣰⣿⣿⣿⣿⣿⢰⣦⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣴⡆⣿⣿⣿⣿⣿⣆⢼ 
⡇⣿⣿⣿⣿⣿⡟⠈⠙⠛⠻⠿⠿⠿⠿⠿⠿⠿⠿⠟⠛⠋⠁⢻⣿⣿⣿⣿⣿⢸ 
⣿⣌⡻⠿⠿⢋⣴⣦⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⢀⣴⣦⡙⠿⠿⢟⣡⣾ 
⣿⣿⣿⣷⣄⠙⢿⣿⣿⣶⣤⣀⡀⡀⡀⡀⡀⡀⣀⣤⣶⣿⣿⡿⠋⣠⣾⣿⣿⣿ 
⣿⣿⣿⣿⣿⣷⣦⣉⠛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⣉⣴⣾⣿⣿⣿⣿⣿ 
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⣌⣉⣉⣉⣉⣉⣉⣡⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿
```

## How this works

All dynamic symbols in a library, e.g. global variables and functions, needs to be resolvable so they can be used by programs that imports the library.  
Linux loader/linker does some optimization by using hashes and bloom filter, which makes the symbol lookup faster. And this bloom filter is placed in .gnu.hash section inside ELF binary.  

Basically each symbol turns on 2 bits somewhere in the bloom filter, and where it turns on depends on the symbol's hash value, it may overlap with other symbol's bits.  
So by crafting symbols with specific hash values it's possible to create arbitrary data inside the bloom filter.  

The linker (or something) adds some own symbols like ```_bss_start```, and these will cause noize while constructing arbitrary data in the bloom filter. bloomify does some effort to find suitable position in the bloom filter that gets least or none incorrect bits caused by these crap symbols.

To learn about how .gnu.hash and bloom filter works, read these links:
* https://blogs.oracle.com/solaris/gnu-hash-elf-sections-v2
* https://flapenguin.me/elf-dt-gnu-hash

Reading these will give you good understanding how this works, maybe.

## How to build

```bash
g++ -O3 -o bloomify bloomify.cc
```

## Usage

### Convert file ```secret``` to c file (saves as ```secret.c```)

```bash
./bloomify secret
```

### Save the generated file as a different name ```bloom.c```

```bash
./bloomify secret -o bloom.c
```

### Generate c file for 32 bit

```bash
./bloomify secret -l
```

### Generate for executable instead of shared library

If you for some reason want to compile as executable, and not shared library, use:
```bash
./bloomify secret -d main
```
This will add some more symbols (e.g. ```main``` and ```_start```) into calculation.  
Then when compiling it with ```gcc``` use ```-rdynamic``` flag:
```bash
gcc -o secret.bin secret.c -rdynamic
```

### Use it in your project

If you for some reason want to combine this with your own project, specify all symbols in your program:
```
./bloomify secret -s func1,func2,func3
```
And then just compile it together with your code, with ```-rdynamic``` for executables ofc.

You can check what kind of dynamic symbols you have using ```readelf``` on your binary:
```
readelf -s -D 
```
(Don't forget to compile with ```-rdynamic``` for executables before checking)

### Use larger bloom size

If you have issue with having many incorrect bits, you can use larger bloom size and hope it will decrease/eliminate errors:
```bash
./bloomify secret -S 2
```
```-S 2``` means twice as large as usual.  

But be careful because this will explode the size, probably exponentially.


## ~~Practical~~ applications

All examples are made for 64 bit.

### CTF

```examples/flag.c```

Convert the flag into c file.

```bash
gcc -o flag.so flag.c -shared
strings flag.so | grep PICHU_CTF
```

### Watermark

```examples/watermark.c```

Sneak in watermark in your code.

```bash
gcc -o watermark.so watermark.c -shared
strings watermark.so | head -n 30
```

### Shellcode

```examples/shellcode.c``` and ```examples/shellcode_main.c```  

For some reason the .gnu.hash section gets placed in a RX (readable executable) segment, which means you can sneak in code there and jump to it from main.  

This example prints hello world, 100% free wirus!!!

```bash
gcc -o shellcode_main shellcode_main.c shellcode.c -rdynamic
./shellcode_main
```

shellcode taken from: https://klaus.hohenpoelz.de/a-simple-yet-complete-shellcode-loader-for-linux-x64-shellcode-part-1.html  
