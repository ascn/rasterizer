#include <stdio.h>
#include <stdlib.h>

#include "ppm.h"
#include "img_t.h"

img_t *read_ppm(const char *fname) {
    FILE *img_file = NULL;
    if (!(img_file = fopen(fname, "rb"))) {
        fprintf(stderr, "error: could not open file %s\n", fname);
    }
    int w = 0;
    int h = 0;
    img_t *img = NULL;
    if (fscanf(img_file, "P6 %d %d 255%*c", &w, &h)) {
        img = img_init(w, h);
    } else {
        fprintf(stderr, "error: invalid ppm file header\n");
        return NULL;
    }

    if (!fread(img->data, 3, w * h, img_file)) {
        fprintf(stderr, "error: invalid ppm file format\n");
        return NULL;
    }

    fclose(img_file);
    return img;
}

int write_ppm(const img_t *img, const char *fname) {
    if (!(img && fname)) {
        fprintf(stderr, "error: illegal argument\n");
        return 0;
    }
    FILE *img_file = NULL;
    if (!(img_file = fopen(fname, "wb"))) {
        fprintf(stderr, "error: could not open file %s\n", fname);
        return 0;
    }
    fprintf(img_file, "P6\n%d %d\n255\n", img->w, img->h);
    fwrite(img->data, img->w * img->h, 3, img_file);

    fclose(img_file);
    return 1;
}