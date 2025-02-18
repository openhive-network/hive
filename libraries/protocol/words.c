#if defined(C23_EMBED_SUPPORTED)

#include <stdint.h>

const uint8_t word_list_zipped[] = {
#embed "words.deflate"
};
const uint32_t word_list_zipped_size = sizeof(word_list_zipped);
static_assert(sizeof(word_list_zipped_size) != 0);

#endif // C23_EMBED_SUPPORTED
