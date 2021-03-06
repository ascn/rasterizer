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
float lerp(float a, float b, float alpha);
float dist2(vec4 p1, vec4 p2);

typedef struct {
    vec4 vert[3];
    vec4 pixel_coord[3];
    vec4 bounding_box[2]; // [0][0] = min x
                          // [0][1] = min y
                          // [1][0] = max x
                          // [1][1] = max y
    bool is_renderable = true;
    pixel_t color;
    vec4 normals[3];
} face_t;

typedef enum { NONE, WHITE, NORM_FLAT, NORM_GOURAUD, NORM_BARY,
               NORM_GOURAUD_Z, NORM_BARY_Z, RANDOM, TEXTURE } e_shader;

int main(int argc, char *argv[]) {

    e_shader shading = NONE;
    std::string text_path;

    if (argc < 6) {
        fprintf(stderr, "usage: rasterize OBJ CAMERA WIDTH HEIGHT OUTPUT OPTION\n");
    } else if (argc > 8) {
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
        } else if (option == "--texture") {
            shading = TEXTURE;
            text_path = argv[7];
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

    std::string mtl_path = "../obj/";

    // LOAD OBJ
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err = tinyobj::LoadObj(shapes, materials, argv[1], mtl_path.c_str());
    if ("" != err) {
    	cout << "error: " << err << endl;
    	exit(1);
    }

    // READ CAMERA.TXT
    camera_mat_t camera = load_camera(argv[2]);

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
            tmp.normals[0] = vec4(mesh.normals[mesh.indices[i] * 3],
                                  mesh.normals[mesh.indices[i] * 3 + 1],
                                  mesh.normals[mesh.indices[i] * 3 + 2], 0);
            tmp.normals[1] = vec4(mesh.normals[mesh.indices[i + 1] * 3],
                                  mesh.normals[mesh.indices[i + 1] * 3 + 1],
                                  mesh.normals[mesh.indices[i + 1] * 3 + 2], 0);
            tmp.normals[2] = vec4(mesh.normals[mesh.indices[i + 2] * 3],
                                  mesh.normals[mesh.indices[i + 2] * 3 + 1],
                                  mesh.normals[mesh.indices[i + 2] * 3 + 2], 0);
            tmp.color = { (unsigned char) (materials[mesh.material_ids[i / 3]].diffuse[0] * 255),
                          (unsigned char) (materials[mesh.material_ids[i / 3]].diffuse[1] * 255),
                          (unsigned char) (materials[mesh.material_ids[i / 3]].diffuse[2] * 255) };
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
            std::swap(f.vert[0], f.vert[1]);
            std::swap(f.normals[0], f.normals[1]);
        }
        if (f.pixel_coord[1][0] > f.pixel_coord[2][0]) {
            std::swap(f.pixel_coord[1], f.pixel_coord[2]);
            std::swap(f.vert[1], f.vert[2]);
            std::swap(f.normals[1], f.normals[2]);
        }
        if (f.pixel_coord[0][0] > f.pixel_coord[1][0]) {
            std::swap(f.pixel_coord[0], f.pixel_coord[1]);
            std::swap(f.vert[0], f.vert[1]);
            std::swap(f.normals[0], f.normals[1]);
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
            f.color = { (unsigned char) ((float) std::rand() / RAND_MAX * 255),
                        (unsigned char) ((float) std::rand() / RAND_MAX * 255),
                        (unsigned char) ((float) std::rand() / RAND_MAX * 255) };
        } else if (shading == NONE) {

        }
    }

    // For each row in the output image
    for (float y = 0.5; y < h; ++y) {
        // Determine which faces intersect the row
        for (auto &f : faces) {
            if (!f.is_renderable) { continue; }
            if (f.bounding_box[0][1] < y && y < f.bounding_box[1][1]) {
                // Row will intersect the triangle
                // Get beginning and ending intersection points

                // Determine if intersects with pixel_coord[0], pixel_coord[1]
                std::vector<float> intersects; // contains two x coordinates where the current face intersects with the row
                std::vector<int> edge_intersects; // contains the indices of the vertices that are intersected
                if (within(y, f.pixel_coord[0][1], f.pixel_coord[1][1])) {
                    // intersects with pixel[0], pixel[1]
                    // coordinates already sorted by increasing x
                    if ((int) f.pixel_coord[0][0] == (int) f.pixel_coord[1][0]) {
                        // vertical line
                        intersects.push_back(f.pixel_coord[0][0]);
                    } else if ((int) f.pixel_coord[0][1] == (int) f.pixel_coord[1][1]) {
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
                    edge_intersects.push_back(0);
                    edge_intersects.push_back(1);
                }

                if (within(y, f.pixel_coord[1][1], f.pixel_coord[2][1])) {
                    if ((int) f.pixel_coord[1][0] == (int) f.pixel_coord[2][0]) {
                        intersects.push_back(f.pixel_coord[1][0]);
                    } else if ((int) f.pixel_coord[1][1] == (int) f.pixel_coord[2][1]) {
                        intersects.push_back(f.pixel_coord[1][0]);
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else {
                        float m = (f.pixel_coord[2][1] - f.pixel_coord[1][1]) /
                                  (f.pixel_coord[2][0] - f.pixel_coord[1][0]);
                        intersects.push_back((y + (m * f.pixel_coord[1][0]) -
                                                f.pixel_coord[1][1]) / m);
                    }
                    edge_intersects.push_back(1);
                    edge_intersects.push_back(2);
                }

                if (within(y, f.pixel_coord[0][1], f.pixel_coord[2][1])) {
                    if ((int) f.pixel_coord[0][0] == (int) f.pixel_coord[2][0]) {
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else if ((int) f.pixel_coord[0][1] == (int) f.pixel_coord[2][1]) {
                        intersects.push_back(f.pixel_coord[0][0]);
                        intersects.push_back(f.pixel_coord[2][0]);
                    } else {
                        float m = (f.pixel_coord[2][1] - f.pixel_coord[0][1]) /
                                  (f.pixel_coord[2][0] - f.pixel_coord[0][0]);
                        intersects.push_back((y + (m * f.pixel_coord[0][0]) -
                                                f.pixel_coord[0][1]) / m);
                    }
                    edge_intersects.push_back(0);
                    edge_intersects.push_back(2);
                }

                if (intersects[0] > intersects[1]) {
                    std::swap(intersects[0], intersects[1]);
                    std::swap(edge_intersects[0], edge_intersects[2]);
                    std::swap(edge_intersects[1], edge_intersects[3]);
                }
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
                    double l0 = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) /
                                ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
                    double l1 = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) /
                                ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
                    double l2 = 1 - l0 - l1;
                    double pix_depth = (1 / f.vert[0][2]) * l0 +
                                       (1 / f.vert[1][2]) * l1 +
                                       (1 / f.vert[2][2]) * l2;
                    pix_depth = 1 / pix_depth;
                    if (pix_depth < z_buf[y * w + i] && within(pix_depth, 0, 1)) {
                        switch (shading) {
                        case NONE:
                            (*out)(y, i) = { f.color.r,
                                             f.color.g,
                                             f.color.b };
                            break;
                        case WHITE:
                            (*out)(y, i) = { 255, 255, 255 };
                            break;
                        case NORM_FLAT: {
                            vec4 view_n_1 = camera.view * f.normals[0];
                            (*out)(y, i) = { (unsigned char) ((view_n_1[0] + 1) * 127.5),
                                             (unsigned char) ((view_n_1[1] + 1) * 127.5),
                                             (unsigned char) ((view_n_1[2] + 1) * 127.5) };
                            break;
                        }
                        case NORM_GOURAUD: {
                            vec4 view_n_1 = camera.view * f.normals[0];
                            vec4 view_n_2 = camera.view * f.normals[1];
                            vec4 view_n_3 = camera.view * f.normals[2];
                            pixel_t c[3];
                            c[0] = { (unsigned char) ((view_n_1[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_1[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_1[2] + 1) * 127.5) };
                            c[1] = { (unsigned char) ((view_n_2[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_2[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_2[2] + 1) * 127.5) };
                            c[2] = { (unsigned char) ((view_n_3[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_3[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_3[2] + 1) * 127.5) };

                            float d1 = dist2(f.pixel_coord[edge_intersects[1]], f.pixel_coord[edge_intersects[0]]);
                            float d2 = dist2(f.pixel_coord[edge_intersects[3]], f.pixel_coord[edge_intersects[2]]);
                            float r1 = lerp(c[edge_intersects[0]].r,
                                            c[edge_intersects[1]].r,
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float g1 = lerp(c[edge_intersects[0]].g,
                                            c[edge_intersects[1]].g,
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float b1 = lerp(c[edge_intersects[0]].b,
                                            c[edge_intersects[1]].b,
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float r2 = lerp(c[edge_intersects[2]].r,
                                            c[edge_intersects[3]].r,
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);
                            float g2 = lerp(c[edge_intersects[2]].g,
                                            c[edge_intersects[3]].g,
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);
                            float b2 = lerp(c[edge_intersects[2]].b,
                                            c[edge_intersects[3]].b,
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);

                            (*out)(y, i) = { (unsigned char) lerp(r1, r2, (i - intersects[0]) / (intersects[1] - intersects[0])),
                                             (unsigned char) lerp(g1, g2, (i - intersects[0]) / (intersects[1] - intersects[0])),
                                             (unsigned char) lerp(b1, b2, (i - intersects[0]) / (intersects[1] - intersects[0])) };
                            break;
                        }
                        case NORM_BARY: {
                            vec4 view_n_1 = camera.view * f.normals[0];
                            vec4 view_n_2 = camera.view * f.normals[1];
                            vec4 view_n_3 = camera.view * f.normals[2];
                            float r[3], g[3], b[3];
                            r[0] = (view_n_1[0] + 1) * 127.5; r[1] = (view_n_2[0] + 1) * 127.5; r[2] = (view_n_3[0] + 1) * 127.5;
                            g[0] = (view_n_1[1] + 1) * 127.5; g[1] = (view_n_2[1] + 1) * 127.5; g[2] = (view_n_3[1] + 1) * 127.5;
                            b[0] = (view_n_1[2] + 1) * 127.5; b[1] = (view_n_2[2] + 1) * 127.5; b[2] = (view_n_3[2] + 1) * 127.5;
                            (*out)(y, i) = { (unsigned char) (r[0] * l0 + r[1] * l1 + r[2] * l2), 
                                             (unsigned char) (g[0] * l0 + g[1] * l1 + g[2] * l2),
                                             (unsigned char) (b[0] * l0 + b[1] * l1 + b[2] * l2) };
                            break;
                        }
                        case NORM_GOURAUD_Z: {
                            vec4 view_n_1 = camera.view * f.normals[0];
                            vec4 view_n_2 = camera.view * f.normals[1];
                            vec4 view_n_3 = camera.view * f.normals[2];
                            pixel_t c[3];
                            c[0] = { (unsigned char) ((view_n_1[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_1[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_1[2] + 1) * 127.5) };
                            c[1] = { (unsigned char) ((view_n_2[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_2[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_2[2] + 1) * 127.5) };
                            c[2] = { (unsigned char) ((view_n_3[0] + 1) * 127.5),
                                     (unsigned char) ((view_n_3[1] + 1) * 127.5),
                                     (unsigned char) ((view_n_3[2] + 1) * 127.5) };

                            float r[3], g[3], b[3];
                            r[0] = c[0].r / f.vert[0][2]; r[1] = c[1].r / f.vert[1][2]; r[2] = c[2].r / f.vert[2][2];
                            g[0] = c[0].g / f.vert[0][2]; g[1] = c[1].g / f.vert[1][2]; g[2] = c[2].g / f.vert[2][2];
                            b[0] = c[0].b / f.vert[0][2]; b[1] = c[1].b / f.vert[1][2]; b[2] = c[2].b / f.vert[2][2];

                            float d1 = dist2(f.pixel_coord[edge_intersects[1]], f.pixel_coord[edge_intersects[0]]);
                            float d2 = dist2(f.pixel_coord[edge_intersects[3]], f.pixel_coord[edge_intersects[2]]);
                            float r1 = lerp(r[edge_intersects[0]], r[edge_intersects[1]],
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float g1 = lerp(g[edge_intersects[0]], g[edge_intersects[1]],
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float b1 = lerp(b[edge_intersects[0]], b[edge_intersects[1]],
                                            dist2(f.pixel_coord[edge_intersects[0]], vec4(intersects[0], y, 0, 0)) / d1);
                            float r2 = lerp(r[edge_intersects[2]], r[edge_intersects[3]],
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);
                            float g2 = lerp(g[edge_intersects[2]], g[edge_intersects[3]],
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);
                            float b2 = lerp(b[edge_intersects[2]], b[edge_intersects[3]],
                                            dist2(f.pixel_coord[edge_intersects[2]], vec4(intersects[1], y, 0, 0)) / d2);

                            (*out)(y, i) = { (unsigned char) (pix_depth * lerp(r1, r2, (i - intersects[0]) / (intersects[1] - intersects[0]))),
                                             (unsigned char) (pix_depth * lerp(g1, g2, (i - intersects[0]) / (intersects[1] - intersects[0]))),
                                             (unsigned char) (pix_depth * lerp(b1, b2, (i - intersects[0]) / (intersects[1] - intersects[0]))) };
                            break;
                        }
                        case NORM_BARY_Z: {
                            vec4 view_n_1 = camera.view * f.normals[0];
                            vec4 view_n_2 = camera.view * f.normals[1];
                            vec4 view_n_3 = camera.view * f.normals[2];
                            float r[3], g[3], b[3];
                            r[0] = (view_n_1[0] + 1) * 127.5; r[1] = (view_n_2[0] + 1) * 127.5; r[2] = (view_n_3[0] + 1) * 127.5;
                            g[0] = (view_n_1[1] + 1) * 127.5; g[1] = (view_n_2[1] + 1) * 127.5; g[2] = (view_n_3[1] + 1) * 127.5;
                            b[0] = (view_n_1[2] + 1) * 127.5; b[1] = (view_n_2[2] + 1) * 127.5; b[2] = (view_n_3[2] + 1) * 127.5;
                            float r_o = (r[0] / f.vert[0][2]) * l0 +
                                        (r[1] / f.vert[1][2]) * l1 + 
                                        (r[2] / f.vert[2][2]) * l2;
                            float g_o = (g[0] / f.vert[0][2]) * l0 +
                                        (g[1] / f.vert[1][2]) * l1 + 
                                        (g[2] / f.vert[2][2]) * l2;
                            float b_o = (b[0] / f.vert[0][2]) * l0 +
                                        (b[1] / f.vert[1][2]) * l1 + 
                                        (b[2] / f.vert[2][2]) * l2;
                            (*out)(y, i) = { (unsigned char) (r_o * pix_depth),
                                             (unsigned char) (g_o * pix_depth),
                                             (unsigned char) (b_o * pix_depth) };
                            break;
                        }
                        case RANDOM:
                            (*out)(y, i) = f.color;
                            break;
                        case TEXTURE:

                        default:
                            fprintf(stderr, "error: option not handled\n");
                            break;
                        }
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

float lerp(float a, float b, float alpha) {
    return a + alpha * (b - a);
}

float dist2(vec4 p1, vec4 p2) {
    return std::sqrt(std::pow((p1[0] - p2[0]), 2) + std::pow((p1[1] - p2[1]), 2));
}
