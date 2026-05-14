#include "internal.h"

#include <string.h>

void mespprc_bitset_init(mespprc_bitset_t* bs, uint64_t* words, int num_bits) {
    bs->words = words;
    bs->num_bits = num_bits;
    bs->num_words = mespprc_bitset_words_for(num_bits);
    if (bs->num_words > 0 && bs->words) {
        memset(bs->words, 0, (size_t)bs->num_words * sizeof(uint64_t));
    }
}

void mespprc_bitset_clear(mespprc_bitset_t* bs) {
    if (bs->num_words > 0 && bs->words) {
        memset(bs->words, 0, (size_t)bs->num_words * sizeof(uint64_t));
    }
}

int mespprc_bitset_get(const mespprc_bitset_t* bs, int bit) {
    if (bit < 0 || bit >= bs->num_bits) return 0;
    return (int)((bs->words[bit / 64] >> (bit % 64)) & 1ULL);
}

void mespprc_bitset_set(mespprc_bitset_t* bs, int bit) {
    if (bit < 0 || bit >= bs->num_bits) return;
    bs->words[bit / 64] |= 1ULL << (bit % 64);
}

void mespprc_bitset_unset(mespprc_bitset_t* bs, int bit) {
    if (bit < 0 || bit >= bs->num_bits) return;
    bs->words[bit / 64] &= ~(1ULL << (bit % 64));
}

#if defined(_MSC_VER)
  #include <intrin.h>
  static int popcount64(uint64_t v) {
    return (int)__popcnt64(v);
  }
#else
  static int popcount64(uint64_t v) {
    return __builtin_popcountll(v);
  }
#endif

int mespprc_bitset_count(const mespprc_bitset_t* bs) {
    int total = 0;
    for (int i = 0; i < bs->num_words; ++i) {
        total += popcount64(bs->words[i]);
    }
    return total;
}

int mespprc_bitset_subset_of(const mespprc_bitset_t* a, const mespprc_bitset_t* b) {
    if (a->num_words != b->num_words) return 0;
    for (int i = 0; i < a->num_words; ++i) {
        if ((a->words[i] & ~b->words[i]) != 0ULL) return 0;
    }
    return 1;
}

int mespprc_bitset_equal(const mespprc_bitset_t* a, const mespprc_bitset_t* b) {
    if (a->num_words != b->num_words) return 0;
    for (int i = 0; i < a->num_words; ++i) {
        if (a->words[i] != b->words[i]) return 0;
    }
    return 1;
}
