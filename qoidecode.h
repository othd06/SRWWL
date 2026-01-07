struct pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct image {
    int w;
    int h;
    struct pixel* data;
};

struct image decodeQOI(const char* filename);

