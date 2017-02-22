#include <iostream>
#include <fstream>
#include <vector>

#include "tiny_obj_loader.h"
#include "vec4.h"
#include "mat4.h"

using namespace std;

int main(int argc, char *argv[]) {
    cout << "argc: " << argc << endl;
    cout << argv[1] << ", " << argv[2] << ", " << argv[3] << endl;

    // LOAD OBJ
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err = tinyobj::LoadObj(shapes, materials, argv[1]);
    if ("" != err) {
    	cout << "error: " << err << endl;
    	exit(1);
    }

    // READ CAMERA.TXT
    std::ifstream camera_file(argv[2]);
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

    cout << left << ", " << right << ", " << top << ", " << bottom << endl;
    cout << near << ", " << far << endl;

    mat4 proj_mat = mat4::proj(left, right, top, bottom, near, far);
    cout << proj_mat << endl;

    return 0;
}
