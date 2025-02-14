#if defined(C23_EMBED_SUPPORTED)

#include <stdint.h>

const uint8_t word_list_zipped[] = {
#embed "words.txt.gz"
};
const uint32_t word_list_zipped_size = sizeof(word_list_zipped);

#endif // C23_EMBED_SUPPORTED
