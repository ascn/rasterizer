#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img_t.h"

img_t *img_init(int w, int h) {
    if (!(w > 0 && h > 0)) {
        fprintf(stderr, "error: cannot initialize image with "
                        "dimensions %d by %d\n", w, h);
        return NULL;
    }
    img_t *img = (img_t *) malloc(sizeof(*img));
    img->w = w;
    img->h = h;

    img->data = (pixel_t *) malloc(w * h * sizeof(*(img->data)));
    memset(img->data, 0, w * h * sizeof(*(img->data)));

    return img;
}

img_t **img_destroy(img_t **img) {
    free((*img)->data);
    (*img)->data = NULL;
    free(*img);
    *img = NULL;
    return img;
}
