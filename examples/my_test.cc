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
#include <libheif/heif_items.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <getopt.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

uint8_t *fromfile(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    uint8_t *data = (uint8_t *)malloc(fsize);
    size_t read_bytes = fread(data, 1, fsize, file);
    assert(read_bytes == (size_t)fsize);
    
    fclose(file);
    *size = fsize;
    return data;
}

void tofile(uint8_t *data, size_t size, const char *path) {
    FILE *file = fopen(path, "wb");
    fwrite(data, size, 1, file);
    fflush(file);
    fclose(file);
}

void print_info(const char *in_path, const char *out_path) {
    heif_context *read_context = heif_context_alloc();
    heif_error error = heif_context_read_from_file(read_context, in_path, nullptr);
    assert(error.code == heif_error_Ok);
    
    int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    heif_context_debug_dump_boxes_to_file(read_context, fd);
    close(fd);
}

void test_code(const char *in_path, const char *out_path) {
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
        
        heif_image *image;
        // brew install x265 libde265
        error = heif_decode_image(handle, &image, heif_colorspace_undefined, heif_chroma_undefined, nullptr);
        assert(error.code == heif_error_Ok);
        
        heif_encoder *encoder;
        heif_context_get_encoder_for_format(write_context, heif_compression_HEVC, &encoder);
        
        heif_encoding_options *options = heif_encoding_options_alloc();
        options->save_alpha_channel = 0;
        
        heif_image_handle *out_handle = nullptr;
        error = heif_context_encode_image(write_context, image, encoder, options, &out_handle);
        assert(error.code == heif_error_Ok);
        
        heif_image_release(image);
        
        int thumbnail_count = heif_image_handle_get_number_of_thumbnails(handle);
        heif_item_id thumbnail_ids[thumbnail_count];
        thumbnail_count = heif_image_handle_get_list_of_thumbnail_IDs(handle, thumbnail_ids, thumbnail_count);
        for (int j = 0; j < thumbnail_count; j++) {
            heif_image_handle *thumbnail_handle = nullptr;
            error = heif_image_handle_get_thumbnail(handle, thumbnail_ids[j], &thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            heif_image *thumbnail_image;
            error = heif_decode_image(thumbnail_handle, &thumbnail_image, heif_colorspace_undefined, heif_chroma_undefined, nullptr);
            assert(error.code == heif_error_Ok);
            
            heif_image_handle *out_thumbnail_handle = nullptr;
            error = heif_context_encode_thumbnail(write_context, thumbnail_image, out_handle, encoder, options, INT_MAX, &out_thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            heif_image_release(thumbnail_image);
            heif_image_handle_release(thumbnail_handle);
            heif_image_handle_release(out_thumbnail_handle);
        }
        heif_encoding_options_free(options);
        heif_encoder_release(encoder);
        
        int metadata_count = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr);
        heif_item_id metadata_ids[metadata_count];
        metadata_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, metadata_ids, metadata_count);
        for (int j = 0; j < metadata_count; j++) {
            heif_item_id metadata_id = metadata_ids[j];
            const char *metadata_type = heif_image_handle_get_metadata_type(handle, metadata_id);
            const char *content_type = heif_image_handle_get_metadata_content_type(handle, metadata_id);
            if (strcmp(metadata_type, "tmap") == 0) {
                continue;
            }
            size_t size = heif_image_handle_get_metadata_size(handle, metadata_id);
            uint8_t data[size];
            error = heif_image_handle_get_metadata(handle, metadata_id, data);
            assert(error.code == heif_error_Ok);
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

void test_handle(const char *in_path, const char *out_path) {
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
        
        heif_image_handle *gain_map_handle;
        error = heif_image_handle_get_gain_map_image_handle(handle, &gain_map_handle);
        if (error.code == heif_error_Ok) {
            heif_item_id item_id = heif_image_handle_get_item_id(gain_map_handle);
            int i = 0;
            for (; i < image_count; i++) {
                if (imageIds[i] == item_id) {
                    break;
                }
            }
            if (i < image_count) {
                for (; i < image_count - 1; i++) {
                    imageIds[i] = imageIds[i + 1];
                }
                image_count--;
            }
        }
        
        heif_image_handle *out_handle = nullptr;
        error = heif_context_add_image(write_context, handle, &out_handle);
        assert(error.code == heif_error_Ok);
        
        int thumbnail_count = heif_image_handle_get_number_of_thumbnails(handle);
        heif_item_id thumbnail_ids[thumbnail_count];
        thumbnail_count = heif_image_handle_get_list_of_thumbnail_IDs(handle, thumbnail_ids, thumbnail_count);
        for (int j = 0; j < thumbnail_count; j++) {
            heif_image_handle *thumbnail_handle = nullptr;
            error = heif_image_handle_get_thumbnail(handle, thumbnail_ids[j], &thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            heif_image_handle *out_thumbnail_handle = nullptr;
            error = heif_context_add_image(write_context, thumbnail_handle, &out_thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            error = heif_context_assign_thumbnail(write_context, out_handle, out_thumbnail_handle);
            assert(error.code == heif_error_Ok);
            
            heif_image_handle_release(thumbnail_handle);
            heif_image_handle_release(out_thumbnail_handle);
        }
        
        int metadata_count = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr);
        heif_item_id metadata_ids[metadata_count];
        metadata_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, metadata_ids, metadata_count);
        for (int j = 0; j < metadata_count; j++) {
            heif_item_id metadata_id = metadata_ids[j];
            const char *metadata_type = heif_image_handle_get_metadata_type(handle, metadata_id);
            const char *content_type = heif_image_handle_get_metadata_content_type(handle, metadata_id);
            if (strcmp(metadata_type, "tmap") == 0) {
                continue;
            }
            size_t size = heif_image_handle_get_metadata_size(handle, metadata_id);
            uint8_t data[size];
            error = heif_image_handle_get_metadata(handle, metadata_id, data);
            assert(error.code == heif_error_Ok);
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

void test_add_jpeg_image(const char *jpeg_in_path, int width, int height, const char *out_path) {
    heif_context *write_context = heif_context_alloc();
    heif_image_handle *out_handle = nullptr;
    
    heif_jpeg_data jpeg_data;
    jpeg_data.data = fromfile(jpeg_in_path, &jpeg_data.size);
    jpeg_data.width = width;
    jpeg_data.height = height;
    
    heif_error error = heif_context_add_jpeg_image(write_context, &jpeg_data, &out_handle);
    assert(error.code == heif_error_Ok);
    free(jpeg_data.data);
    
    heif_image_handle_release(out_handle);
    
    error = heif_context_write_to_file(write_context, out_path);
    assert(error.code == heif_error_Ok);
    heif_context_free(write_context);
}

void test_get_jpeg_data(const char *in_path, const char *jpeg_out_path) {
    heif_context *read_context = heif_context_alloc();
    heif_error error = heif_context_read_from_file(read_context, in_path, nullptr);
    assert(error.code == heif_error_Ok);
    
    int image_count = heif_context_get_number_of_top_level_images(read_context);
    heif_item_id imageIds[image_count];
    image_count = heif_context_get_list_of_top_level_image_IDs(read_context, imageIds, image_count);
    assert(image_count > 0);

    heif_image_handle *handle = nullptr;
    error = heif_context_get_image_handle(read_context, imageIds[0], &handle);
    assert(error.code == heif_error_Ok);
    
    struct heif_jpeg_data jpeg_data;
    error = heif_get_jpeg_data(handle, &jpeg_data);
    assert(error.code == heif_error_Ok);
    
    tofile(jpeg_data.data, jpeg_data.size, jpeg_out_path);
    free(jpeg_data.data);
}

void test_add_metadata(const char *in_path, const char *target_metadata_type, const char *metadata_path, const char *out_path) {
    heif_context *read_context = heif_context_alloc();
    heif_error error = heif_context_read_from_file(read_context, in_path, nullptr);
    assert(error.code == heif_error_Ok);
    
    heif_context *write_context = heif_context_alloc();
    
    int image_count = heif_context_get_number_of_top_level_images(read_context);
    heif_item_id imageIds[image_count];
    image_count = heif_context_get_list_of_top_level_image_IDs(read_context, imageIds, image_count);
    assert(image_count == 1);

    heif_image_handle *handle = nullptr;
    error = heif_context_get_image_handle(read_context, imageIds[0], &handle);
    assert(error.code == heif_error_Ok);
    
    heif_image_handle *out_handle = nullptr;
    error = heif_context_add_image(write_context, handle, &out_handle);
    assert(error.code == heif_error_Ok);
    
    int thumbnail_count = heif_image_handle_get_number_of_thumbnails(handle);
    heif_item_id thumbnail_ids[thumbnail_count];
    thumbnail_count = heif_image_handle_get_list_of_thumbnail_IDs(handle, thumbnail_ids, thumbnail_count);
    for (int j = 0; j < thumbnail_count; j++) {
        heif_image_handle *thumbnail_handle = nullptr;
        error = heif_image_handle_get_thumbnail(handle, thumbnail_ids[j], &thumbnail_handle);
        assert(error.code == heif_error_Ok);
        
        heif_image_handle *out_thumbnail_handle = nullptr;
        error = heif_context_add_image(write_context, thumbnail_handle, &out_thumbnail_handle);
        assert(error.code == heif_error_Ok);
        
        error = heif_context_assign_thumbnail(write_context, out_handle, out_thumbnail_handle);
        assert(error.code == heif_error_Ok);
        
        heif_image_handle_release(thumbnail_handle);
        heif_image_handle_release(out_thumbnail_handle);
    }
    
    int metadata_count = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr);
    heif_item_id metadata_ids[metadata_count];
    metadata_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, metadata_ids, metadata_count);
    for (int j = 0; j < metadata_count; j++) {
        heif_item_id metadata_id = metadata_ids[j];
        const char *metadata_type = heif_image_handle_get_metadata_type(handle, metadata_id);
        const char *content_type = heif_image_handle_get_metadata_content_type(handle, metadata_id);
        if (strcmp(metadata_type, target_metadata_type) == 0) {
            continue;
        }
        size_t size = heif_image_handle_get_metadata_size(handle, metadata_id);
        uint8_t data[size];
        error = heif_image_handle_get_metadata(handle, metadata_id, data);
        assert(error.code == heif_error_Ok);
        error = heif_context_add_generic_metadata(write_context, out_handle, data, (int)size, metadata_type, content_type);
        assert(error.code == heif_error_Ok);
    }
    size_t metadata_size = 0;
    uint8_t *metadata = fromfile(metadata_path, &metadata_size);
    
    error = heif_context_add_generic_metadata(write_context, out_handle, metadata, (int)metadata_size, target_metadata_type, nullptr);
    assert(error.code == heif_error_Ok);
    
    free(metadata);
    
    if (heif_image_handle_is_primary_image(handle)) {
        heif_context_set_primary_image(write_context, out_handle);
    }
    heif_image_handle_release(handle);
    heif_image_handle_release(out_handle);

    heif_context_free(read_context);
    error = heif_context_write_to_file(write_context, out_path);
    assert(error.code == heif_error_Ok);
    heif_context_free(write_context);
}

void test_get_metadata(const char *in_path, const char *target_metadata_type, const char *metadata_out_path) {
    heif_context *read_context = heif_context_alloc();
    heif_error error = heif_context_read_from_file(read_context, in_path, nullptr);
    assert(error.code == heif_error_Ok);
    
    int image_count = heif_context_get_number_of_top_level_images(read_context);
    heif_item_id imageIds[image_count];
    image_count = heif_context_get_list_of_top_level_image_IDs(read_context, imageIds, image_count);
    assert(image_count == 1);

    heif_image_handle *handle = nullptr;
    error = heif_context_get_image_handle(read_context, imageIds[0], &handle);
    assert(error.code == heif_error_Ok);
    
    int count = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr);
    assert(count > 0);
    
    heif_item_id metadata_ids[count];
    int filled_count = heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, metadata_ids, count);
    assert(filled_count == count);
    for (int i = 0; i < count; i++) {
        heif_item_id metadata_id = metadata_ids[i];
        const char *metadata_type = heif_image_handle_get_metadata_type(handle, metadata_id);
        if (strcmp(metadata_type, target_metadata_type) == 0) {
            size_t size = heif_image_handle_get_metadata_size(handle, metadata_id);
            uint8_t bytes[size];
            error = heif_image_handle_get_metadata(handle, metadata_id, bytes);
            assert(error.code == heif_error_Ok);
            
            tofile(bytes, size, metadata_out_path);
            return;
        }
    }
    assert(0);
}

void test_update_exif(const char *in_path, const char *exif_in_path, const char *out_path) {
    test_add_metadata(in_path, "Exif", exif_in_path, out_path);
}

void test_get_exif(const char *in_path, const char *exif_out_path) {
    test_get_metadata(in_path, "Exif", exif_out_path);
}

int main(int argc, char** argv) {
    test_code("../../../examples/C034.heic", "C034_code_out.heic");
    test_handle("../../../examples/C034.heic", "C034_handle_out.heic");
    test_add_jpeg_image("../../../examples/sample_1920×1280.jpeg", 1920, 1280, "sample_1920×1280_out.heic");
    test_get_jpeg_data("sample_1920×1280_out.heic", "sample_1920×1280_out.jpeg");
    test_add_metadata("../../../examples/IMG_20240923_194805.HEIC", "Sub", "../../../examples/SubMetaData.jpeg", "IMG_20240923_194805_sub_out.heic");
    test_get_metadata("IMG_20240923_194805_sub_out.heic", "Sub", "SubMetaData_out.jpeg");
    test_update_exif("../../../examples/IMG_20240923_194805.HEIC", "../../../examples/IMG_20240923_194805.EXIF", "IMG_20240923_194805_out.heic");
    test_get_exif("IMG_20240923_194805_out.heic", "IMG_20240923_194805_out.exif");
    return 0;
}
