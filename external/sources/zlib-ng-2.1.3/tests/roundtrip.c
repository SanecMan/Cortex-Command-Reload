#include "zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_roundtrip(uLong size, int level, int repetitive) {
    const uLong bound = compressBound(size);
    Bytef* input = malloc(size ? size : 1);
    Bytef* output = malloc(size ? size : 1);
    Bytef* packed = malloc(bound);
    int passed = 0;
    if (!input || !output || !packed) goto done;
    unsigned int random = 0x12345678U;
    for (uLong index = 0; index < size; ++index) {
        random = random * 1664525U + 1013904223U;
        input[index] = repetitive ? (Bytef)(index % 17) : (Bytef)(random >> 24);
    }
    uLong packed_size = bound;
    uLong output_size = size;
    if (compress2(packed, &packed_size, input, size, level) != Z_OK) goto done;
    if (uncompress(output, &output_size, packed, packed_size) != Z_OK) goto done;
    if (output_size != size || memcmp(input, output, size) != 0) goto done;
    if (crc32(0, input, size) != crc32(0, output, output_size)) goto done;
    output_size = size;
    if (uncompress(output, &output_size, packed, packed_size / 2) == Z_OK) goto done;
    passed = 1;
done:
    free(input);
    free(output);
    free(packed);
    if (!passed) fprintf(stderr, "roundtrip failed: size=%lu level=%d repetitive=%d\n", size, level, repetitive);
    return passed;
}

int main(void) {
    const uLong sizes[] = {0, 1, 255, 65536, 1048576};
    const int levels[] = {0, 1, 6, 9};
    for (size_t size = 0; size < sizeof(sizes) / sizeof(sizes[0]); ++size)
        for (size_t level = 0; level < sizeof(levels) / sizeof(levels[0]); ++level)
            for (int repetitive = 0; repetitive < 2; ++repetitive)
                if (!check_roundtrip(sizes[size], levels[level], repetitive)) return EXIT_FAILURE;
    puts("zlib compatibility roundtrip: passed (40 cases)");
    return EXIT_SUCCESS;
}
