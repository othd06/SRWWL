

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "qoidecode.h"

void error(const char* error_name) {
    fprintf(stderr, "Error: ");
    fprintf(stderr, error_name);
    fprintf(stderr, "\n");
    exit(1);
}

uint8_t* loadFile(const char* filename, size_t* outSize) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        error("file unsuccessfully opened");
    }

    // find file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    // allocate buffer
    uint8_t* buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    // read into buffer
    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(buffer);
        return NULL;
    }

    *outSize = size;
    return buffer;
}

struct image decodeQOI(const char* filename) {
    //load the image data
    size_t size;
    uint8_t* qoi_data = loadFile(filename, &size);
    int index = 0;

    //helper functions
    uint8_t pop() {
        if (index >= size) error("trying to pop from qoi_data beyond end");
        index += 1;
        return qoi_data[index-1];
    }

    //verify header
    assert(pop() == 'q');
    assert(pop() == 'o');
    assert(pop() == 'i');
    assert(pop() == 'f');
    uint32_t width = 0;
    for (int i = 0; i < 4; i++) {
        width = width << 8;
        width += pop();
    }
    uint32_t height = 0;
    for (int i = 0; i < 4; i++) {
        height = height << 8;
        height += pop();
    }
    {
        int x = pop();
        assert(x == 3 || x == 4);
        x = pop();
        assert(x == 0 || x == 1);
    }

    //create image and allocate pixel buffer
    struct image img = {
        .w = width,
        .h = height,
        .data = malloc(width*height*sizeof(struct pixel))
    };

    //decode the pixel data
    struct pixel seen[64];
    for (int i = 0; i < 64; i++) seen[i] = (struct pixel){0, 0, 0, 0};
    struct pixel prev = (struct pixel){0, 0, 0, 255};
    int imgIndex = 0;
    void push(struct pixel pix) {
        if (imgIndex >= img.w*img.h) error("decode failed: trying to overwrite image buffer");
        img.data[imgIndex] = pix;
        imgIndex++;
        seen[(pix.r * 3 + pix.g * 5 + pix.b * 7 + pix.a * 11) % 64] = pix;}
    while (index < (size-8)) {
        uint8_t newByte = pop();
        if (newByte == 0xFE) {
            prev.r = pop();
            prev.g = pop();
            prev.b = pop();
            prev.a = 255;
            push(prev);
        } else if (newByte == 0xFF) {
            prev.r = pop();
            prev.g = pop();
            prev.b = pop();
            prev.a = pop();
            push(prev);
        } else if (newByte >> 6 == 0) {
            prev = seen[newByte];
            push(prev);
        } else if (newByte >> 6 == 1) {
            prev.r -= 2;
            prev.g -= 2;
            prev.b -= 2;
            prev.b += newByte & 0x03;
            prev.g += (newByte & 0x0C) >> 2;
            prev.r += (newByte & 0x30) >> 4;
            push(prev);
        } else if (newByte >> 6 == 2) {
            uint8_t byte2 = pop();
            prev.r -= 40;
            prev.g -= 32;
            prev.b -= 40;
            prev.r += (newByte & 0x3F) + ((byte2 & 0xF0) >> 4);
            prev.g += newByte & 0x3F;
            prev.b += (newByte & 0x3F) + (byte2 & 0x0F);
            push(prev);
        } else {
            for (int i = 0; i < ((newByte & 0x3F) + 1); i++) {push(prev);}
        }
    }

    //verify footer
    for (int i = 0; i < 7; i++) {assert(pop() == 0);}
    assert(pop() == 1);

    //free the data
    free(qoi_data);

    //return the image
    return img;
}


