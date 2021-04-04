/*
 * Running with valgrind. Make sure DCMAKE_BUILD_TYPE is set to debug. Works best if set in CMakeLists.txt.
 * cmake --build . -- -o0 && valgrind --leak-check=yes --track-origins=yes -s ./playground
 */

#include <stdio.h>

#include "valor.h" 

int main(int argc, char** argv) {
	if(argc == 1) {
		fprintf(stderr, "Usage: ./yc <file>\n");
		return -1;
	}

	find_valors(argv[1]);

	return 0;
}
