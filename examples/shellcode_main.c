#include <string.h>

int main()
{
	// start searching backward from main's address
	char *shellcode = (char *)main;
	// marker to find in the memory, obfuscated to avoid accidentaly matching
    // with the string used for this variable
	char marker[] = "SHELLC0DE";
	marker[6] = 'O ';
	while (memcmp(shellcode, marker, 9))
		shellcode--;
	// run the shellcode
	(*(void(*)())(shellcode + 9))();
	return 0;
}
