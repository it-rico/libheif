#include "heif_jpeg_data.h"
#include "api_structs.h"


heif_error heif_get_jpeg_data(const heif_image_handle* in_handle, heif_jpeg_data *out_data)
{
  heif_item_id id = in_handle->image->get_id();
  
  out_data->width = heif_image_handle_get_width(in_handle);
  out_data->height = heif_image_handle_get_height(in_handle);
  
  Error err = in_handle->context->get_jpeg_data(id, out_data);
  if (err.error_code != heif_error_Ok) {
    return err.error_struct(in_handle->image.get());
  }
  
  return Error::Ok.error_struct(in_handle->image.get());
}


heif_error heif_context_add_jpeg_image(heif_context* ctx,
                                       const heif_jpeg_data *data,
                                       heif_image_handle** out_image_handle)
{
  std::vector<uint8_t> jpeg_data(data->data, data->data + data->size);
  auto addResult = ctx->context->add_jpeg_image(ctx->context, jpeg_data, data->width, data->height);
  if (addResult.error) {
    return addResult.error.error_struct(ctx->context.get());
  }
  std::shared_ptr<ImageItem> image = addResult.value;
  
  // mark the new image as primary image
  if (ctx->context->is_primary_image_set() == false) {
    ctx->context->set_primary_image(image);
  }
  
  if (out_image_handle) {
    heif_image_handle *handle = new heif_image_handle;
    handle->image = image;
    handle->context = ctx->context;
    *out_image_handle = handle;
  }
  
  return heif_error_success;
}
