#include "vp9_subexponential.h"

#define MAX_PROB 255

struct bin{
    u32 string;
    u8 cnt;
};


static const u8 map_table[MAX_PROB - 1] = {
     20,  21,  22,  23,  24,  25,   0,  26,  27,  28,
     29,  30,  31,  32,  33,  34,  35,  36,  37,   1,
     38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
     48,  49,   2,  50,  51,  52,  53,  54,  55,  56,
     57,  58,  59,  60,  61,   3,  62,  63,  64,  65,
     66,  67,  68,  69,  70,  71,  72,  73,   4,  74,
     75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
     85,   5,  86,  87,  88,  89,  90,  91,  92,  93,
     94,  95,  96,  97,   6,  98,  99, 100, 101, 102,
    103, 104, 105, 106, 107, 108, 109,   7, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
      8, 122, 123, 124, 125, 126, 127, 128, 129, 130,
    131, 132, 133,   9, 134, 135, 136, 137, 138, 139,
    140, 141, 142, 143, 144, 145,  10, 146, 147, 148,
    149, 150, 151, 152, 153, 154, 155, 156, 157,  11,
    158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
    168, 169,  12, 170, 171, 172, 173, 174, 175, 176,
    177, 178, 179, 180, 181,  13, 182, 183, 184, 185,
    186, 187, 188, 189, 190, 191, 192, 193,  14, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
    205,  15, 206, 207, 208, 209, 210, 211, 212, 213,
    214, 215, 216, 217,  16, 218, 219, 220, 221, 222,
    223, 224, 225, 226, 227, 228, 229,  17, 230, 231,
    232, 233, 234, 235, 236, 237, 238, 239, 240, 241,
     18, 242, 243, 244, 245, 246, 247, 248, 249, 250,
    251, 252, 253,  19
};

static const struct bin sub_exp[MAX_PROB - 1] = {
    {0x0,5}, {0x1,5}, {0x2,5}, {0x3,5}, {0x4,5},
    {0x5,5}, {0x6,5}, {0x7,5}, {0x8,5}, {0x9,5},
    {0xA,5}, {0xB,5}, {0xC,5}, {0xD,5}, {0xE,5},
    {0xF,5}, {0x20,6}, {0x21,6}, {0x22,6}, {0x23,6},
    {0x24,6}, {0x25,6}, {0x26,6}, {0x27,6}, {0x28,6},
    {0x29,6}, {0x2A,6}, {0x2B,6}, {0x2C,6}, {0x2D,6},
    {0x2E,6}, {0x2F,6}, {0xC0,8}, {0xC1,8}, {0xC2,8},
    {0xC3,8}, {0xC4,8}, {0xC5,8}, {0xC6,8}, {0xC7,8},
    {0xC8,8}, {0xC9,8}, {0xCA,8}, {0xCB,8}, {0xCC,8},
    {0xCD,8}, {0xCE,8}, {0xCF,8}, {0xD0,8}, {0xD1,8},
    {0xD2,8}, {0xD3,8}, {0xD4,8}, {0xD5,8}, {0xD6,8},
    {0xD7,8}, {0xD8,8}, {0xD9,8}, {0xDA,8}, {0xDB,8},
    {0xDC,8}, {0xDD,8}, {0xDE,8}, {0xDF,8}, {0x380,10},
    {0x381,10}, {0x382,10}, {0x383,10}, {0x384,10}, {0x385,10},
    {0x386,10}, {0x387,10}, {0x388,10}, {0x389,10}, {0x38A,10},
    {0x38B,10}, {0x38C,10}, {0x38D,10}, {0x38E,10}, {0x38F,10},
    {0x390,10}, {0x391,10}, {0x392,10}, {0x393,10}, {0x394,10},
    {0x395,10}, {0x396,10}, {0x397,10}, {0x398,10}, {0x399,10},
    {0x39A,10}, {0x39B,10}, {0x39C,10}, {0x39D,10}, {0x39E,10},
    {0x39F,10}, {0x3A0,10}, {0x3A1,10}, {0x3A2,10}, {0x3A3,10},
    {0x3A4,10}, {0x3A5,10}, {0x3A6,10}, {0x3A7,10}, {0x3A8,10},
    {0x3A9,10}, {0x3AA,10}, {0x3AB,10}, {0x3AC,10}, {0x3AD,10},
    {0x3AE,10}, {0x3AF,10}, {0x3B0,10}, {0x3B1,10}, {0x3B2,10},
    {0x3B3,10}, {0x3B4,10}, {0x3B5,10}, {0x3B6,10}, {0x3B7,10},
    {0x3B8,10}, {0x3B9,10}, {0x3BA,10}, {0x3BB,10}, {0x3BC,10},
    {0x3BD,10}, {0x3BE,10}, {0x3BF,10}, {0x3C0,10}, {0x782,11},
    {0x783,11}, {0x784,11}, {0x785,11}, {0x786,11}, {0x787,11},
    {0x788,11}, {0x789,11}, {0x78A,11}, {0x78B,11}, {0x78C,11},
    {0x78D,11}, {0x78E,11}, {0x78F,11}, {0x790,11}, {0x791,11},
    {0x792,11}, {0x793,11}, {0x794,11}, {0x795,11}, {0x796,11},
    {0x797,11}, {0x798,11}, {0x799,11}, {0x79A,11}, {0x79B,11},
    {0x79C,11}, {0x79D,11}, {0x79E,11}, {0x79F,11}, {0x7A0,11},
    {0x7A1,11}, {0x7A2,11}, {0x7A3,11}, {0x7A4,11}, {0x7A5,11},
    {0x7A6,11}, {0x7A7,11}, {0x7A8,11}, {0x7A9,11}, {0x7AA,11},
    {0x7AB,11}, {0x7AC,11}, {0x7AD,11}, {0x7AE,11}, {0x7AF,11},
    {0x7B0,11}, {0x7B1,11}, {0x7B2,11}, {0x7B3,11}, {0x7B4,11},
    {0x7B5,11}, {0x7B6,11}, {0x7B7,11}, {0x7B8,11}, {0x7B9,11},
    {0x7BA,11}, {0x7BB,11}, {0x7BC,11}, {0x7BD,11}, {0x7BE,11},
    {0x7BF,11}, {0x7C0,11}, {0x7C1,11}, {0x7C2,11}, {0x7C3,11},
    {0x7C4,11}, {0x7C5,11}, {0x7C6,11}, {0x7C7,11}, {0x7C8,11},
    {0x7C9,11}, {0x7CA,11}, {0x7CB,11}, {0x7CC,11}, {0x7CD,11},
    {0x7CE,11}, {0x7CF,11}, {0x7D0,11}, {0x7D1,11}, {0x7D2,11},
    {0x7D3,11}, {0x7D4,11}, {0x7D5,11}, {0x7D6,11}, {0x7D7,11},
    {0x7D8,11}, {0x7D9,11}, {0x7DA,11}, {0x7DB,11}, {0x7DC,11},
    {0x7DD,11}, {0x7DE,11}, {0x7DF,11}, {0x7E0,11}, {0x7E1,11},
    {0x7E2,11}, {0x7E3,11}, {0x7E4,11}, {0x7E5,11}, {0x7E6,11},
    {0x7E7,11}, {0x7E8,11}, {0x7E9,11}, {0x7EA,11}, {0x7EB,11},
    {0x7EC,11}, {0x7ED,11}, {0x7EE,11}, {0x7EF,11}, {0x7F0,11},
    {0x7F1,11}, {0x7F2,11}, {0x7F3,11}, {0x7F4,11}, {0x7F5,11},
    {0x7F6,11}, {0x7F7,11}, {0x7F8,11}, {0x7F9,11}, {0x7FA,11},
    {0x7FB,11}, {0x7FC,11}, {0x7FD,11}, {0x7FE,11}
};


i32 recenter_nonneg(i32 update, i32 old)
{
    if (update > (old << 1)) {
        return update;
    } else if (update >= old) {
        return ((update - old) << 1);
    } else {
        return ((old - update) << 1) - 1;
    }
}

i32 remap_prob(i32 update, i32 old)
{
    i32 i;

    update--;
    old--;

    if ((old << 1) <= MAX_PROB) {
        i = recenter_nonneg(update, old);
    } else {
        i = recenter_nonneg(MAX_PROB - 1 - update, MAX_PROB - 1 - old);
    }

    return map_table[i - 1];
}


void get_se(i32 update, i32 old, i32 *value, i32 *number)
{
    i32 i = remap_prob(update, old);

    *value = sub_exp[i].string;
    *number = sub_exp[i].cnt;
}

