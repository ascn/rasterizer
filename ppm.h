#ifndef __PPM_H__
#define __PPM_H__

#include "img_t.h"

/*
 * Read a *.ppm file and return the corresponding img_t * struct.
 * If the reading is unsuccessful, NULL is returned.
 */
img_t *read_ppm(const char *fname);

/*
 * Write an img_t to fname. If the write is unsuccessful, 0
 * is returned.
 */
int write_ppm(const img_t *img, const char *fname);

#endif