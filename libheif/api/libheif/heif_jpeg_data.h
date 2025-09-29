#ifndef LIBHEIF_HEIF_JPEG_DATA_H
#define LIBHEIF_HEIF_JPEG_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>
#include <libheif/heif_image.h>

typedef struct heif_jpeg_data {
  uint8_t *data;
  size_t size;
  uint32_t width;
  uint32_t height;
} heif_jpeg_data;

LIBHEIF_API
heif_error heif_get_jpeg_data(const heif_image_handle* in_handle, heif_jpeg_data *out_data);

LIBHEIF_API
heif_error heif_context_add_jpeg_image(heif_context* ctx,
                                       const heif_jpeg_data *data,
                                       heif_image_handle** out_image_handle);

#ifdef __cplusplus
}
#endif

#endif
