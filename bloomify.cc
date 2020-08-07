// ported from python script, which was 16+ times slower

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define CPU_MASK ((cpu_bits) - 1)
#define CPU_BYTES ((cpu_bits) / 8)

uint32_t cpu_bits = 64;
uint32_t cpu_shift = 6;

// character used in symbol name, basically [a-zA-Z_][a-zA-Z0-9_]*
const char *initial_char = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
const char *symbol_char = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

// hash algorithm used for .gnu.hash
uint32_t djb_hash(const char *name)
{
	uint32_t hash = 5381;
	while (*name)
		hash = hash * 33 + *(name++);

	return hash;
}

// "distance" between two hashes, lower the more close to each other
uint32_t hash_diff(uint32_t h1, uint32_t h2)
{
	uint32_t diff = h1 - h2;
	if (h2 - h1 < diff)
		diff = h2 - h1;

	return diff;
}

// get index of blooms from hash
uint32_t hash_id(uint32_t hash, uint32_t bloom_size)
{
	return (hash >> cpu_shift) & (bloom_size - 1);
}

// get bit1 in blooms from hash
uint32_t hash_bit_1(uint32_t hash)
{
	return hash & CPU_MASK;
}

// get bit2 in blooms from hash
uint32_t hash_bit_2(uint32_t hash, uint32_t shift)
{
	return (hash >> shift) & CPU_MASK;
}

// check if the bit is bloomed (set) in blooms
bool check_bloom_bit(uint8_t *blooms, uint32_t id, uint32_t bit)
{
	return blooms[id * CPU_BYTES + bit / 8] & (1 << (bit % 8));
}

// bloom (set) the bit in blooms
bool set_bloom_bit(uint8_t *blooms, uint32_t id, uint32_t bit)
{
	bool flipped = !check_bloom_bit(blooms, id, bit);
	blooms[id * CPU_BYTES + bit / 8] |= 1 << (bit % 8);
	return flipped;
}

// calculate the bloom shift value from total symbols needed
uint32_t calc_shift(uint32_t total_symbols)
{
	uint32_t i = total_symbols / 2;
	uint32_t shift = 1;

	// kinda like log2
	while (i > 0) {
		shift++;
		i /= 2;
	}
	// check bit at "most significant bit that is on - 1" in total_symbols
	// kinda like rounding up decimals
	if (total_symbols & (1 << (shift - 2)))
		shift++;
	shift += 2;

	// must be at least log2(cpu_bits) = cpu_shift
    // because that amount of bits are needed to address one bit in a bloom
	if (shift < cpu_shift)
		shift = cpu_shift;

	return shift;
}

// count amount of bits that are on
uint32_t count_bits(uint8_t *data, uint32_t size)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < size; i++) {
		for (uint32_t j = 0; j < 8; j++) {
			if (data[i] & (1 << j))
				count++;
		}
	}

	return count;
}

// calculate amount of symbols needed to construct data within blooms
uint32_t calc_symbols(uint8_t *data, uint32_t size, uint32_t slide)
{
	uint32_t total_symbols = 0;
	uint32_t i = 0;

	if (slide > 0) {
		uint32_t count = count_bits(data + i * CPU_BYTES, MIN(size, CPU_BYTES - slide));
		total_symbols += (count + 1) / 2;

		data += MIN(size, CPU_BYTES - slide);
		size -= MIN(size, CPU_BYTES - slide);
	}

	for (uint32_t i = 0; i < (size + (CPU_BYTES - 1)) / CPU_BYTES; i++) {
		uint32_t count = count_bits(data + i * CPU_BYTES, MIN(CPU_BYTES, size - i * CPU_BYTES));
		total_symbols += (count + 1) / 2;
	}

	return total_symbols;
}

// generate random symbol name that can be used in c
void random_symbol(char *symbol, uint32_t length)
{
	if (!length)
		return;

	symbol[0] = initial_char[rand() % strlen(initial_char)];
	for (uint32_t i = 1; i < length - 1; i++)
		symbol[i] = symbol_char[rand() % strlen(symbol_char)];

	symbol[length - 1] = 0;
}

// find a symbol name that has the specified hash value
void hash_collide(uint32_t hash, char *symbol, uint32_t length)
{
	for (;;) {
		uint32_t diff = -1;
		random_symbol(symbol, length);

		for (uint32_t i = 0; i < length - 1; i++) {
			uint32_t c = 0;
			const char *charset = symbol_char;
			if (i == 0)
				charset = initial_char;

			diff = -1;
			for (uint32_t j = 0; j < strlen(charset); j++) {
				symbol[i] = charset[j];

				uint32_t test_diff = hash_diff(hash, djb_hash(symbol));
				if (test_diff < diff) {
					diff = test_diff;
					c = charset[j];
				}
			}

			symbol[i] = c;
		}

		if (!diff)
			break;
	}
}

// -h
void help(char *bin) {
	fprintf(stderr, "usage: %s [option] <source-file>\n\n", bin);
	fprintf(stderr, "\t-o\t\toutput file, appends \".c\" to source if not specified\n");
	fprintf(stderr, "\t-0\t\tturn off slide, has more error bits but faster, probably\n");
	fprintf(stderr, "\t-d <value>\tsymbol-sets to include in calculation, set to shared by default\n");
	fprintf(stderr, "\t\t\tshared: include \"_bss_start\", \"_end\", \"_edata\", \"_init\", \"_fini\"\n");
	fprintf(stderr, "\t\t\tmain: include \"__bss_start\", \"main\", \"_init\", \"__libc_csu_fini\", \"_fini\", \"_edata\", \"__data_start\", \"_end\", \"data_start\", \"_IO_stdin_used\", \"__libc_csu_init\", \"_start\"\n");
	fprintf(stderr, "\t\t\tempty: don't use any shits\n");
	fprintf(stderr, "\t-s <values,...>\tinclude own symbols in calculation, specify in comma separated format\n");
	fprintf(stderr, "\t\t\t  e.g. \"-s foo,bar\"\n");
	fprintf(stderr, "\t-l\tgenerate for 32 bit, 64 bit if not specified\n");
	fprintf(stderr, "\t-S <value>\tscale up the bloom size, must be power of two (not 1)\n");
}

// yes
int main(int argc, char **argv)
{
	char *source_path = 0;
	char *target_path = 0;

	bool no_slide = false;
	const char *symbol_sets = "shared";
	const char *user_symbol_sets = 0;
	uint32_t scaling = 1;

	// parse arguments
	int opt = 0;
	while((opt = getopt(argc, argv, ":h0d:s:o:lS:")) != -1) {
		switch (opt) {
			case 'h':
				help(argv[0]);
				exit(0);
			case '0':
				no_slide = true;
				break;
			case 'd':
				symbol_sets = optarg;
				break;
			case 's':
				user_symbol_sets = optarg;
				break;
			case 'o':
				target_path = optarg;
				break;
			case 'l':
				cpu_bits = 32;
				cpu_shift = 5;
				break;
			case 'S':
				scaling = atol(optarg);
				if ((scaling & (scaling - 1)) || scaling < 2) {
					fprintf(stderr, "error: not power of two\n");
					exit(1);
				}
				break;
			case ':':
				fprintf(stderr, "error: -%c needs a value\n", optopt);
				exit(1);
				break;
			case '?':
				fprintf(stderr, "error: unknown flag -%c\n", optopt);
				exit(1);
				break;
		}
	}

	if (optind < argc) {
		source_path = argv[optind];
	} else {
		help(argv[0]);
		exit(1);
	}

	// crappy symbols included by linker or whatever
	std::vector<const char *> default_symbols;

	if (!strcmp(symbol_sets, "shared")) {
		default_symbols.push_back("__bss_start");
		default_symbols.push_back("_end");
		default_symbols.push_back("_edata");
		default_symbols.push_back("_init");
		default_symbols.push_back("_fini");
	} else if (!strcmp(symbol_sets, "main")) {
		default_symbols.push_back("_edata");
		default_symbols.push_back("__data_start");
		default_symbols.push_back("_end");
		default_symbols.push_back("data_start");
		default_symbols.push_back("_IO_stdin_used");
		default_symbols.push_back("__libc_csu_init");
		default_symbols.push_back("_start");
		default_symbols.push_back("__bss_start");
		default_symbols.push_back("main");
		default_symbols.push_back("_init");
		default_symbols.push_back("__libc_csu_fini");
		default_symbols.push_back("_fini");
	} else if (strcmp(symbol_sets, "empty")) {
		fprintf(stderr, "error: unknown symbol-set\n");
		exit(1);
	}

	// add user specified craps
	if (user_symbol_sets) {
		char *symbols = strdup(user_symbol_sets);
		char *symbol = 0;
		while ((symbol = strsep(&symbols, ",")))
			default_symbols.push_back(symbol);
		free(symbols);
	}

	// open file to convert
	uint32_t file_size = 0;
	FILE *file = fopen(source_path, "rb");
	if (!file) {
		fprintf(stderr, "error: failed to open file\n");
		exit(1);
	}

	// create target file to save shit to
	if (!target_path) {
		// memory-leak.com
		target_path = (char *)malloc(strlen(source_path) + 3);
		sprintf(target_path, "%s.c", source_path);
	}

	FILE *target_file = fopen(target_path, "w");
	if (!target_file) {
		fprintf(stderr, "error: failed to create target file\n");
		exit(1);
	}

	// get file size
	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	// read content
	uint8_t *data = (uint8_t *)malloc(file_size);
	uint32_t bytes_read = fread(data, 1, file_size, file);
	fclose(file);

	if (file_size != bytes_read) {
		fprintf(stderr, "error: failed to read whole file\n");
		free(data);
		exit(1);
	}

	// all generated symbols needs to be unique
	std::set<std::string> used_symbols;

	for (const char *symbol : default_symbols)
		used_symbols.insert(symbol);

	// begin real shits
	uint32_t total_symbols = 0;
	uint32_t padding_symbols = 0;
	uint32_t shift = 0;
	uint32_t bloom_size = 0;

	uint32_t slide = 0;
	uint32_t start_id = 0;
	uint32_t least_incorrect_bits = -1;

	uint32_t previous_bloom_size = 0;
	uint8_t *test_blooms = 0;

	// big fat loop to find optimal position in blooms that has least error bits
	for (uint32_t test_slide = 0; test_slide < CPU_BYTES; test_slide++) {
		uint32_t test_total_symbols = calc_symbols(data, file_size, test_slide) + default_symbols.size();
		uint32_t test_padding_symbols = 0;
		uint32_t test_shift = calc_shift(test_total_symbols);
		uint32_t test_bloom_size = 1 << (test_shift - cpu_shift);
		uint32_t minimum_bloom_size = (1 << int(ceil(log2(file_size)))) / CPU_BYTES;

		// file containing only nulls
		if (test_total_symbols - default_symbols.size() == 0) {
			fprintf(stderr, "error: file with only nulls not allowed\n");
			exit(1);
		}

		// low effort way to force scaling up bloom size
		if (scaling != 1)
			minimum_bloom_size = MAX(minimum_bloom_size, test_bloom_size) * scaling;

		// check if the estimated bloom size is large enough
		// if not recalculate shits using the minimum required size
		if (test_bloom_size < minimum_bloom_size) {
			test_bloom_size = minimum_bloom_size;
			// i don't remember how i got this formula, feels incorrect but it works somehow
			test_padding_symbols = 1 << (int(log2(test_bloom_size)) + (cpu_bits == 64 ? 2 : 1));
			test_padding_symbols += test_padding_symbols / 2;
			test_padding_symbols -= test_total_symbols;
			test_shift = calc_shift(test_total_symbols + test_padding_symbols);
		}

		// bloom all default symbols first time, and reuse it if the bloom size has not changed
		// very rare case but bloom size may differ depending on slide value
		if (previous_bloom_size != test_bloom_size) {
			free(test_blooms);
			test_blooms = (uint8_t *)calloc(test_bloom_size, CPU_BYTES);

			for (const char *name : default_symbols) {
				uint32_t hash = djb_hash(name);
				uint32_t id = hash_id(hash, test_bloom_size);
				uint32_t b1 = hash_bit_1(hash);
				uint32_t b2 = hash_bit_2(hash, test_shift);

				set_bloom_bit(test_blooms, id, b1);
				set_bloom_bit(test_blooms, id, b2);
			}
		}
		previous_bloom_size = test_bloom_size;

		uint32_t test_start_id = 0;

		// calculate amount of error bits
		while (test_start_id * CPU_BYTES + test_slide + file_size <= test_bloom_size * CPU_BYTES) {
			uint32_t incorrect_bits = 0;
			for (uint32_t i = 0; i < file_size; i++) {
				for (uint32_t j = 0; j < 8; j++) {
					// more loops more fun
					if (test_blooms[test_start_id * CPU_BYTES + test_slide + i] & (1 << j) && !(data[i] & (1 << j)))
						incorrect_bits++;
				}
			}

			// if it seems good use it
			if (incorrect_bits < least_incorrect_bits) {
				least_incorrect_bits = incorrect_bits;
				start_id = test_start_id;
				slide = test_slide;

				total_symbols = test_total_symbols;
				padding_symbols = test_padding_symbols;
				shift = test_shift;
				bloom_size = test_bloom_size;
			}

			// if no error bits are found, then no need to search further
			if (!least_incorrect_bits)
				break;

			test_start_id++;
		}

		// bye-bye
		if (!least_incorrect_bits)
			break;

		if (no_slide)
			break;
	}
	free(test_blooms);

	// default symbols are used implicitly so subtract them
	total_symbols -= default_symbols.size();

	// print some useless statistics
	printf("data symbols: %d\n", total_symbols);
	printf("padding symbols: %d\n", padding_symbols);

	printf("bloom size: %d\n", bloom_size);
	printf("bloom shift: %d\n", shift);
	printf("start id: %d\n", start_id);
	printf("slide: %d\n", slide);

	if (least_incorrect_bits > 0)
		printf("\x1b[41mwarning: %d bits will be incorrect\x1b[0m\n", least_incorrect_bits);

	// allocate a lot
	uint8_t *blooms = (uint8_t *)calloc(bloom_size, CPU_BYTES);
	uint8_t *current_blooms = (uint8_t *)malloc(bloom_size * CPU_BYTES);
	uint8_t *bits_left = (uint8_t *)calloc(bloom_size, 1);
	uint32_t total_bits = 0;

	memcpy(blooms + start_id * CPU_BYTES + slide, data, file_size);
	// set to 0 for the region where file data lies, and rest to ff
	// this makes a little bit faster finding padding symbols, maybe
	memset(current_blooms, 0xff, bloom_size * CPU_BYTES);
	memset(current_blooms + start_id * CPU_BYTES + slide, 0, file_size);

	for (uint32_t i = 0; i < bloom_size * CPU_BYTES; i++) {
		for (uint32_t j = 0; j < 8; j++) {
			if (blooms[i] & (1 << j)) {
				total_bits++;
				bits_left[i / CPU_BYTES]++;
			}
		}
	}

	srand(time(0));

	const uint32_t brute_tries = 2500;

	const uint32_t symbol_length = 16 + 1;
	char *symbol = (char *)malloc(symbol_length);

	// start brute forcing hashes and hope they correspond to file content bits
	printf("%-8s %8s %2s %2s %-9s %-9s %-32s\n", "hash", "id", "b1", "b2", "bits-left", "pad-syms", "symbol-name");
	while (total_bits > 0 || padding_symbols > 0) {
		uint32_t hash = 0;
		uint32_t id = 0;
		uint32_t b1 = 0;
		uint32_t b2 = 0;
		uint32_t misses = -1;

		while (++misses < brute_tries) {
			// cannot switch method until all padding symbols has been generated
		 	// why? cuz i'm lazy
			if (padding_symbols > 0)
				misses = 0;
			random_symbol(symbol, symbol_length);

			hash = djb_hash(symbol);
			id = hash_id(hash, bloom_size);
			b1 = hash_bit_1(hash);
			b2 = hash_bit_2(hash, shift);

			bool cur_b1_on = check_bloom_bit(current_blooms, id, b1);
			bool cur_b2_on = check_bloom_bit(current_blooms, id, b2);
			bool b1_on = check_bloom_bit(blooms, id, b1);
			bool b2_on = check_bloom_bit(blooms, id, b2);

			if (padding_symbols > 0) {
				// uninteresting bits? use them for padding!
				if (cur_b1_on && cur_b2_on && used_symbols.find(symbol) == used_symbols.end()) {
					padding_symbols--;
					// inc total symbols to correct the counting
					total_symbols++;
					break;
				}
			}

			// already found all needed bits in this index
			if (!bits_left[id])
				continue;

			// when there is only one bit left in a bloom element to find
			if (bits_left[id] == 1) {
				if (!cur_b1_on && b1_on && b2_on)
					break;
				if (!cur_b2_on && b2_on && b1_on)
					break;

				continue;
			}

			// both bits are not same and corresponds to two bits needed
			if (b1 != b2 && !cur_b1_on && !cur_b2_on && b1_on && b2_on)
				break;
		}

		// switch to hash collision method when it gets too many misses
		if (misses == brute_tries)
			break;

		// update current blooms
		total_symbols--;
		if (set_bloom_bit(current_blooms, id, b1)) {
			bits_left[id]--;
			total_bits--;
		}
		if (set_bloom_bit(current_blooms, id, b2)) {
			bits_left[id]--;
			total_bits--;
		}

		// update used symbols
		used_symbols.insert(symbol);

		// write the symbol to file
		fprintf(target_file, "int %s;\n", symbol);
		// print cool but still useless statistics
		printf("\x0d%08x %8d %2d %2d %09d %09d %-32s", hash, id, b1, b2, total_bits, padding_symbols, symbol);
	}

	// use hash collision method if brute force was shit
	if (total_bits > 0) {
		for (uint32_t id = 0; id < bloom_size; id++) {
			while (bits_left[id]) {
				uint32_t b1 = -1;
				uint32_t b2 = -1;
				// find bits that needs to be bloomed
				for (uint32_t b = 0; b < cpu_bits; b++) {
					if (!check_bloom_bit(current_blooms, id, b) &&
						check_bloom_bit(blooms, id, b)) {
						if (b1 == (uint32_t)-1) {
							b1 = b;
							b2 = b;
						} else {
							b2 = b;
							break;
						}
					}
				}

				// wtf
				assert(b1 != (uint32_t)-1);
				assert(b2 != (uint32_t)-1);

				// find symbol name with this hash value
				uint32_t hash = b1 | (b2 << shift) | (id << cpu_shift);
				hash_collide(hash, symbol, symbol_length);

				// bloom it
				if (set_bloom_bit(current_blooms, id, b1)) {
					bits_left[id]--;
					total_bits--;
				}
				if (set_bloom_bit(current_blooms, id, b2)) {
					bits_left[id]--;
					total_bits--;
				}

				used_symbols.insert(symbol);

				total_symbols--;

				fprintf(target_file, "int %s;\n", symbol);
				printf("\x0d%08x %8d %2d %2d %09d %09d %-32s", hash, id, b1, b2, total_bits, padding_symbols, symbol);
			}
		}
	}
	printf("\n");

	printf("written to file: %s\n", target_path);

	// assert.
	for (uint32_t id = 0; id < bloom_size; id++)
		assert(bits_left[id] == 0);
	assert(total_symbols == 0);
	assert(total_bits == 0);
	assert(padding_symbols == 0);

	// free.
	free(symbol);
	free(current_blooms);
	free(blooms);
	free(data);

	return 0;
}
