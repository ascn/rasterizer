#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "mat4.h"

typedef struct {
	mat4 proj;
	mat4 view;
} camera_mat_t;

camera_mat_t load_camera(char *file);

#endif