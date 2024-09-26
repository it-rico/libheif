/*
  libheif example application "heif-test".

  MIT License

  Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <errno.h>
#include <string.h>

#if defined(HAVE_UNISTD_H)

#include <unistd.h>

#else
#define STDOUT_FILENO 1
#endif

#include <libheif/heif.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <getopt.h>
#include <assert.h>
#include <stdio.h>

void tofile(uint8_t *data, size_t size, const char *path) {
    FILE *file = fopen(path, "wb");
    fwrite(data, size, 1, file);
    fflush(file);
    fclose(file);
}

void test(const char *in_path, const char *out_path) {
    heif_context *read_context = heif_context_alloc();
    heif_error error = heif_context_read_from_file(read_context, in_path, nullptr);
    assert(error.code == heif_error_Ok);
    
    heif_context *write_context = heif_context_alloc();
    int image_count = heif_context_get_number_of_top_level_images(read_context);
    heif_item_id imageIds[image_count];
    image_count = heif_context_get_list_of_top_level_image_IDs(read_context, imageIds, image_count);
    
    for(int i = 0; i < image_count; i++) {
        heif_image_handle *handle = nullptr;
        error = heif_context_get_image_handle(read_context, imageIds[i], &handle);
        assert(error.code == heif_error_Ok);
        
        heif_image_data *image_data;
        error = heif_get_image_data(handle, &image_data);
        assert(error.code == heif_error_Ok);
        
        heif_image_handle *out_handle = nullptr;
        error = heif_context_add_image_data(write_context, image_data, &out_handle);
        assert(error.code == heif_error_Ok);
        
        heif_image_data_release(image_data);
        
        int thumbnail_count = heif_image_handle_get_number_of_thumbnails(handle);
        heif_item_id thumbnail_ids[thumbnail_count];
        thumbnail_count = heif_image_handle_get_list_of_thumbnail_IDs(handle, thumbnail_ids, thumbnail_count);
        for (int j = 0; j < thumbnail_count; j++) {
            heif_image_handle *thumbnail_handle = nullptr;
            error = heif_image_handle_get_thumbnail(handle, thumbnail_ids[j], &thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            
            heif_image_data *thumbnail_image_data;
            error = heif_get_image_data(thumbnail_handle, &thumbnail_image_data);
            assert(error.code == heif_error_Ok);
            
            heif_image_handle *out_thumbnail_handle = nullptr;
            error = heif_context_add_image_data(write_context, thumbnail_image_data, &out_thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            error = heif_context_assign_thumbnail(write_context, out_handle, out_thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            heif_image_data_release(thumbnail_image_data);
            heif_image_handle_release(thumbnail_handle);
            heif_image_handle_release(out_thumbnail_handle);
        }
        
        int metadata_count = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr);
        heif_item_id metadata_ids[metadata_count];
        metadata_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, metadata_ids, metadata_count);
        for (int j = 0; j < metadata_count; j++) {
            heif_item_id metadata_id = metadata_ids[j];
            size_t size = heif_image_handle_get_metadata_size(handle, metadata_id);
            uint8_t data[size];
            error = heif_image_handle_get_metadata(handle, metadata_id, data);
            assert(error.code == heif_error_Ok);
            const char *metadata_type = heif_image_handle_get_metadata_type(handle, metadata_id);
            const char *content_type = heif_image_handle_get_metadata_content_type(handle, metadata_id);
            error = heif_context_add_generic_metadata(write_context, out_handle, data, (int)size, metadata_type, content_type);
            assert(error.code == heif_error_Ok);
        }
        if (heif_image_handle_is_primary_image(handle)) {
            heif_context_set_primary_image(write_context, out_handle);
        }
        heif_image_handle_release(handle);
        heif_image_handle_release(out_handle);
    }
    heif_context_free(read_context);
    error = heif_context_write_to_file(write_context, out_path);
    assert(error.code == heif_error_Ok);
    heif_context_free(write_context);
}

int main(int argc, char** argv) {
//    test("/Users/jaelyn/Downloads/heics/input.heic", "/Users/jaelyn/Downloads/heics/output.heic");
    //    test("/Users/jaelyn/Downloads/heics/C034.heic", "/Users/jaelyn/Downloads/heics/output.heic");
    //    test("/Users/jaelyn/Downloads/heics/exif.heic", "/Users/jaelyn/Downloads/heics/exif_output.heic");
//    test("/Users/jaelyn/Downloads/IMG_5050.HEIC", "/Users/jaelyn/Downloads/heics/IMG_5050_output.heic");
    test("/Users/jaelyn/Downloads/heics/IMG_5050_output.heic", "/Users/jaelyn/Downloads/heics/IMG_5050_output2.heic");
//    test("/Users/jaelyn/Downloads/heics/output.heic", "/Users/jaelyn/Downloads/heics/output2.heic");
    return 0;
}
