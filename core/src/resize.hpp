#ifndef LIPI_SRC_RESIZE_HPP
#define LIPI_SRC_RESIZE_HPP

#include <vector>

#include "lipi/lipi.h"

namespace lipi {

// Area-averages the image to out_w x out_h and returns planar RGB floats
// (all of R, then G, then B), values 0..255. Gray images are replicated and
// alpha is ignored. This matches OpenCV's INTER_AREA, which the model was
// trained with.
std::vector<float> resize_area_planar_rgb(const lipi_image_view& src, int out_w, int out_h);

}  // namespace lipi

#endif
