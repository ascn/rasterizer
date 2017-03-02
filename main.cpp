#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>

#include "tiny_obj_loader.h"
#include "camera.h"
#include "vec4.h"
#include "mat4.h"
#include "ppm.h"

using namespace std;

bool within(float y, float c1, float c2);

typedef struct {
    vec4 vert[3];
    vec4 pixel_coord[3];
    vec4 bounding_box[2]; // [0][0] = min x
                          // [0][1] = min y
                          // [1][0] = max x
                          // [1][1] = max y
    bool is_renderable = true;
    pixel_t color;
} face_t;

typedef enum { NONE, WHITE, NORM_FLAT, NORM_GOURAUD, NORM_BARY,
               NORM_GOURAUD_Z, NORM_BARY_Z, RANDOM } e_shader;

int main(int argc, char *argv[]) {

    const float EPSILON = 0.0001;
    e_shader shading;

    if (argc < 6) {
        fprintf(stderr, "usage: rasterize OBJ CAMERA WIDTH HEIGHT OUTPUT OPTION\n");
    } else if (argc > 7) {
        fprintf(stderr, "usage: please specify only one shading option\n");
    } else if (argc == 7) {
        std::string option(argv[6]);
        if (option == "--white") {
            shading = WHITE;
        } else if (option == "--norm_flat") {
            shading = NORM_FLAT;
        } else if (option == "--norm_gouraud") {
            shading = NORM_GOURAUD;
        } else if (option == "--norm_bary") {
            shading = NORM_BARY;
        } else if (option == "--norm_gouraud_z") {
            shading = NORM_GOURAUD_Z;
        } else if (option == "--norm_bary_z") {
            shading = NORM_BARY_Z;
        } else if (option == "--random") {
            shading = RANDOM;
        }
    } else {
        shading = NONE;
    }

    int w = std::stoi(argv[3]);
    int h = std::stoi(argv[4]);

    // Initialize image
    img_t *out = img_init(w, h);

    // Initialize z-buffer
    std::vector<double> z_buf;
    for (int i = 0; i < w * h; ++i) {
        z_buf.push_back(2);
    }

    std::string mtl_path = "C:\Users\achan\Projects\cis560\rasterizer\rasterizer\obj";

    // LOAD OBJ
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err = tinyobj::LoadObj(shapes, materials, argv[1]);
    if ("" != err) {
    	cout << "error: " << err << endl;
    	exit(1);
    }

//    shapes.clear();
//    materials.clear();
//    tinyobj::shape_t tmp;
//    tinyobj::mesh_t tmp_mesh;
//    tmp_mesh.positions.push_back(-.5);
//    tmp_mesh.positions.push_back(-.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.positions.push_back(-.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.positions.push_back(.5);
//    tmp_mesh.indices.push_back(0);
//    tmp_mesh.indices.push_back(1);
//    tmp_mesh.indices.push_back(2);
//    tmp.mesh = tmp_mesh;
//    shapes.push_back(tmp);

    // READ CAMERA.TXT
    camera_mat_t camera = load_camera(argv[2]);
    cout << camera.proj << endl;
    cout << camera.view << endl;

    std::vector<face_t> faces;
    for (const auto &s : shapes) {
        tinyobj::mesh_t mesh = s.mesh;
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            face_t tmp;
            tmp.vert[0] = vec4(mesh.positions[mesh.indices[i] * 3],
                               mesh.positions[mesh.indices[i] * 3 + 1],
                               mesh.positions[mesh.indices[i] * 3 + 2], 1);
            tmp.vert[1] = vec4(mesh.positions[mesh.indices[i + 1] * 3],
                               mesh.positions[mesh.indices[i + 1] * 3 + 1],
                               mesh.positions[mesh.indices[i + 1] * 3 + 2], 1);
            tmp.vert[2] = vec4(mesh.positions[mesh.indices[i + 2] * 3],
                               mesh.positions[mesh.indices[i + 2] * 3 + 1],
                               mesh.positions[mesh.indices[i + 2] * 3 + 2], 1);
            tmp.color = { materials[mesh.material_ids[i / 3]].diffuse[0] * 255,
                          materials[mesh.material_ids[i / 3]].diffuse[1] * 255,
                          materials[mesh.material_ids[i / 3]].diffuse[2] * 255 };
            faces.push_back(tmp);
        }
    }

    for (auto &f : faces) {
        f.vert[0] = camera.proj * camera.view * f.vert[0];
        f.vert[1] = camera.proj * camera.view * f.vert[1];
        f.vert[2] = camera.proj * camera.view * f.vert[2];
        f.vert[0] /= f.vert[0][3];
        f.vert[1] /= f.vert[1][3];
        f.vert[2] /= f.vert[2][3];
        // [x, y, 0, 0]
        f.pixel_coord[0] = vec4((f.vert[0][0] + 1) * (w / 2),
                                (1 - f.vert[0][1]) * (h / 2), 0, 0);
        f.pixel_coord[1] = vec4((f.vert[1][0] + 1) * (w / 2),
                                (1 - f.vert[1][1]) * (h / 2), 0, 0);
        f.pixel_coord[2] = vec4((f.vert[2][0] + 1) * (w / 2),
                                (1 - f.vert[2][1]) * (h / 2), 0, 0);
        if ((f.vert[0][2] < 0 && f.vert[1][2] < 0 && f.vert[2][2] < 0) ||
                (f.vert[0][2] > 1 && f.vert[1][2] > 1 && f.vert[2][2] > 1)) {
            f.is_renderable = false;
        }
        if (f.pixel_coord[0][0] > f.pixel_coord[1][0]) {
            std::swap(f.pixel_coord[0], f.pixel_coord[1]);
        }
        if (f.pixel_coord[1][0] > f.pixel_coord[2][0]) {
            std::swap(f.pixel_coord[1], f.pixel_coord[2]);
        }
        if (f.pixel_coord[0][0] > f.pixel_coord[1][0]) {
            std::swap(f.pixel_coord[0], f.pixel_coord[1]);
        }
        f.bounding_box[0][0] = min(f.pixel_coord[0][0],
                               min(f.pixel_coord[1][0], f.pixel_coord[2][0]));
        f.bounding_box[0][1] = min(f.pixel_coord[0][1],
                               min(f.pixel_coord[1][1], f.pixel_coord[2][1]));
        f.bounding_box[1][0] = max(f.pixel_coord[0][0],
                               max(f.pixel_coord[1][0], f.pixel_coord[2][0]));
        f.bounding_box[1][1] = max(f.pixel_coord[0][1],
                               max(f.pixel_coord[1][1], f.pixel_coord[2][1]));
        if (f.bounding_box[0][0] > w || f.bounding_box[0][1] > h ||
                f.bounding_box[1][0] < 0 || f.bounding_box[1][1] < 0) {
            f.is_renderable = false;
        }
        if (shading == RANDOM) {
            f.color = { (float) std::rand() / RAND_MAX * 255,
                        (float) std::rand() / RAND_MAX * 255,
                        (float) std::rand() / RAND_MAX * 255 };
        } else if (shading == NONE) {

        }
    }

    // For each row in the output image
    for (float y = 0.5; y < h; ++y) {
        // Determine which faces intersect the row
        std::vector<float> intersections;
        for (auto &f : faces) {
            if (!f.is_renderable) { continue; }
            if (f.bounding_box[0][1] < y && y < f.bounding_box[1][1]) {
                // Row will intersect the triangle
                // Get beginning and ending intersection points

                // Determine if intersects with pixel_coord[0], pixel_coord[1]
                std::vector<float> intersects; // contains two x coordinates where the current face intersects with the row
                bool horizontal = false;
                if (within(y, f.pixel_coord[0][1], f.pixel_coord[1][1])) {
                    // intersects with pixel[0], pixel[1]
                    // coordinates already sorted by increasing x
                    if ((int) f.pixel_coord[0][0] == (int) f.pixel_coord[1][0]) {
                        // vertical line
                        intersects.push_back(f.pixel_coord[0][0]);
                    } else if ((int) f.pixel_coord[0][1] == (int) f.pixel_coord[1][1]) {
                        // horizontal line
                        horizontal = true;
                        intersects.push_back(f.pixel_coord[0][0]);
                        intersects.push_back(f.pixel_coord[1][0]);
                    } else {
                        // regular case, intersection testing
                        float m = (f.pixel_coord[1][1] - f.pixel_coord[0][1]) /
                                  (f.pixel_coord[1][0] - f.pixel_coord[0][0]);
                        // y = m * X - m * pixel_coord[0][0] + pixel_coord[0][1]
                        // (y + (m * pixel_coord[0][0]) - pixel_coord[0][1]) / m = X
                        intersects.push_back((y + (m * f.pixel_coord[0][0]) -
                                                f.pixel_coord[0][1]) / m);
                    }
                }

                if (within(y, f.pixel_coord[1][1], f.pixel_coord[2][1])) {
                    if ((int) f.pixel_coord[1][0] == (int) f.pixel_coord[2][0]) {
                        intersects.push_back(f.pixel_coord[1][0]);
                    } else if ((int) f.pixel_coord[1][1] == (int) f.pixel_coord[2][1]) {
                        horizontal = true;
                        intersects.push_back(f.pixel_coord[1][0]);
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else {
                        float m = (f.pixel_coord[2][1] - f.pixel_coord[1][1]) /
                                  (f.pixel_coord[2][0] - f.pixel_coord[1][0]);
                        intersects.push_back((y + (m * f.pixel_coord[1][0]) -
                                                f.pixel_coord[1][1]) / m);
                    }
                }

                if (within(y, f.pixel_coord[0][1], f.pixel_coord[2][1])) {
                    if ((int) f.pixel_coord[0][0] == (int) f.pixel_coord[2][0]) {
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else if ((int) f.pixel_coord[0][1] == (int) f.pixel_coord[2][1]) {
                        horizontal = true;
                        intersects.push_back(f.pixel_coord[0][0]);
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else {
                        float m = (f.pixel_coord[2][1] - f.pixel_coord[0][1]) /
                                  (f.pixel_coord[2][0] - f.pixel_coord[0][0]);
                        intersects.push_back((y + (m * f.pixel_coord[0][0]) -
                                                f.pixel_coord[0][1]) / m);
                    }
                }

                std::sort(intersects.begin(), intersects.end());
                // interpolate depth for intersects[0] and intersects[1]

                /* Render wireframe */

//                if (horizontal) {
//                    for (int i = intersects[0]; i <= intersects[1]; ++i) {
//                        (*out)(y, i) = f.color;
//                    }
//                }

//                if (intersects[0] >= 0 && intersects[0] < w) {
//                    (*out)(y, intersects[0]) = f.color;
//                }
//                if (intersects[1] >=0 && intersects[1] < w) {
//                    (*out)(y, intersects[1]) = f.color;
//                }

                // y * h + i
                double x1 = f.pixel_coord[0][0];
                double x2 = f.pixel_coord[1][0];
                double x3 = f.pixel_coord[2][0];
                double y1 = f.pixel_coord[0][1];
                double y2 = f.pixel_coord[1][1];
                double y3 = f.pixel_coord[2][1];
                for (int i = intersects[0]; i < intersects[1]; ++i) {
                    if (i < 0 || i > w) { continue; }
                    // Get barycentric coordinate of pixel
                    double x = i;
                    double l0 = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
                    double l1 = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
                    double l2 = 1 - l0 - l1;
                    double pix_depth = (1 / f.vert[0][2]) * l0 + (1 / f.vert[1][2]) * l1 + (1 / f.vert[2][2]) * l2;
                    pix_depth = 1 / pix_depth;
                    if (pix_depth < z_buf[y * w + i] && within(pix_depth, 0, 1)) {
                        switch (shading) {
                        case NONE:

                            (*out)(y, i) = { f.color.r * pix_depth,
                                             f.color.g * pix_depth,
                                             f.color.b * pix_depth };
                            break;
                        case WHITE:
                            (*out)(y, i) = { 255, 255, 255 };
                            break;
                        case RANDOM:
                            (*out)(y, i) = f.color;
                            break;
                        }
                        //(*out)(y, i) = f.color;
                        z_buf[y * w + i] = pix_depth;
                    }
                }

            }
        }
    }

    // Write image
    write_ppm(out, argv[5]);

    return 0;
}

bool within(float y, float c1, float c2) {
    return (c1 <= y && y <= c2) || (c2 <= y && y <= c1);
}
