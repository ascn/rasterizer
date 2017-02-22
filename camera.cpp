#include <ifstream>

#include "camera.h"

camera_mat_t load_camera(char *file) {
	std::ifstream camera_file(file);
    if (!camera_file.is_open()) {
    	fprintf(stderr, "error: cannot open file %s\n", argv[2]);
    	exit(1);
    }
    float left, right, top, bottom, near, far;
    camera_file >> left;
    camera_file >> right;
    camera_file >> top;
    camera_file >> bottom;
    camera_file >> near;
    camera_file >> far;
}