#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include "tiny_obj_loader.h"
#include "camera.h"
#include "vec4.h"
#include "mat4.h"
#include "ppm.h"

using namespace std;

typedef struct {
    vec4 vert[3];
    vec4 pixel_coord[3];
    vec4 bounding_box[2]; // [0][0] = min x
                          // [0][1] = min y
                          // [1][0] = max x
                          // [1][1] = max y
    bool is_renderable = true;
} face_t;

int main(int argc, char *argv[]) {

    if (argc < 6) {
        fprintf(stderr, "usage: rasterize OBJ CAMERA WIDTH HEIGHT OUTPUT\n");
    }

    int w = std::stoi(argv[3]);
    int h = std::stoi(argv[4]);

    // LOAD OBJ
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err = tinyobj::LoadObj(shapes, materials, argv[1]);
    if ("" != err) {
    	cout << "error: " << err << endl;
    	exit(1);
    }

    // READ CAMERA.TXT
    camera_mat_t camera = load_camera(argv[2]);
    cout << camera.proj << endl;
    cout << camera.view << endl;

    /**
    For each triangle, multiply each vertex coordinate by the view matrix, then the projection matrix, divide through by w, and convert the x- and y- coordinates to pixel coordinates using the formula from the slides.

    If all three z-coordinates are less than 0 or all three are greater than 1, the triangle is completely in front of the near plane or complete behind the far plane. So skip it because it isn't visible.

    Compute the 2D bounding box of the triangle. If it doesn't overlap the image at all, skip the triangle because it isn't visible.

    For every row of the image, calculate the x-coordinates where the row crosses each edge of the triangle. Figure out which two edges it actually crosses.

    Using the formula on the lecture slides, pixels are actually treated as small squares, just like they really are. The top-left pixel in the image is a square that stretches from (0, 0) to (1, 1). You should use scan-lines that cross through the middle of each pixel, so the y-values of your scanlines should be 0.5, 1.5, 2.5, ..., height - 0.5.

    Be careful of special cases, including vertical edges (could cause a divide-by-zero if you're not careful), horizontal edges (the scan line could run along the entire edge instead of just intersecting it), and situations where the scan line intersects a vertex (i.e. it intersects two triangle edges at exactly the same place). You do not have to deal with degenerate triangle (e.g. triangles where two vertices are actually the same point, or where all three vertices lies along a straight line).
    Once you've calculated the x-coordinates where the current scan line enters and exits the triangle, color every pixel that is at least partially covered by the triangle using its diffuse color (specified in the material struct).

    */

    std::vector<face_t> faces;
    for (const auto &s : shapes) {
        tinyobj::mesh_t mesh = s.mesh;
        for (int i = 0; i < mesh.indices.size(); i += 3) {
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
            faces.push_back(tmp);
        }
    }

    for (auto &f : faces) {
        f.vert[0] = f.vert[0] * camera.view * camera.proj;
        f.vert[1] = f.vert[1] * camera.view * camera.proj;
        f.vert[2] = f.vert[2] * camera.view * camera.proj;
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
        f.bounding_box[0][0] = min(f.pixel_coord[0][0],
                               min(f.pixel_coord[1][0], f.pixel_coord[2][0]));
        f.bounding_box[0][1] = min(f.pixel_coord[0][1],
                               min(f.pixel_coord[1][1], f.pixel_coord[2][1]));
        f.bounding_box[1][0] = max(f.pixel_coord[0][0],
                               max(f.pixel_coord[1][0], f.pixel_coord[2][0]));
        f.bounding_box[1][1] = max(f.pixel_coord[0][1],
                               max(f.pixel_coord[1][1], f.pixel_coord[2][1]));
        if (f.bouding_box[0][0] > w || f.bouding_box[0][1] > h ||
                f.bounding_box[1][0] < 0 || f.bounding_box[1][1] < 0) {
            f.is_renderable = false;
        }
    }

    // For each row in the output image
    for (float y = 0.5; y < h; ++y) {
        // Determine which faces intersect the row
        for (auto &f : faces) {

        }
    }

    return 0;
}
