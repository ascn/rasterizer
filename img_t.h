#ifndef __IMG_T_H__
#define __IMG_T_H__

typedef struct pixel_t {
    unsigned char r, g, b;
} pixel_t;

typedef struct img_t {
    pixel_t *data;
    int w, h;
} img_t;

/*
 * Initialize a new img_t with corresponding height and width.
 * If creation is unsuccessful, NULL is returned.
 */
img_t *img_init(int w, int h);

/*
 * Destroys an img_t. If successful, NULL is returned,
 */
img_t **img_destroy(img_t **img);

#endif