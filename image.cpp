//
// Created by michal on 05.09.20.
//

#include "image.h"

#include <png.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

#include <climits>
#include <cmath>

GImage::GImage(const std::filesystem::path &filename)
{
    std::ifstream input_file(filename, std::ios::binary);

    if (!input_file)
        throw std::runtime_error("Failed to open file: " + filename.string());

    auto err_func = [] (png_structp png_ptr, png_const_charp error_msg) -> void {
        std::cerr << error_msg << std::endl;
    };

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, err_func, err_func);

    if (png == nullptr)
        throw std::runtime_error("Failed to create the PNG read struct.");

    png_rw_ptr read_func = [] (png_structp png_ptr, png_bytep data, size_t length) -> void {
        auto *input_file = static_cast<std::ifstream *>(png_get_io_ptr(png_ptr));
        input_file->read(reinterpret_cast<char *>(data), length);
    };

    png_set_read_fn(png, &input_file, read_func);

    png_infop info = png_create_info_struct(png);

    if (info == nullptr)
        throw std::runtime_error("Failed to create the PNG info struct.");

    png_read_info(png, info);

    this->width = png_get_image_width(png, info);
    this->height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type & PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0)
        png_set_tRNS_to_alpha(png);

    png_color_16 bgcolor;
    png_set_background(png, &bgcolor, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);

    if (color_type & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGBA)
        png_set_rgb_to_gray_fixed(png, PNG_ERROR_ACTION_NONE, -1, -1);

    if (color_type & PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    png_read_update_info(png, info);

    this->bitmap = new unsigned char[png_get_rowbytes(png, info) * this->height];

    for (uint32_t i = 0; i < this->height; i++)
        png_read_row(png, reinterpret_cast<png_bytep>(&this->bitmap[i * width]), nullptr);

    png_destroy_read_struct(&png, &info, nullptr);
}

GImage::GImage(uint32_t widthIn, uint32_t heightIn) : width(widthIn), height(heightIn)
{
    if (this->width > GImage::MAX_SIZE || this->height > GImage::MAX_SIZE)
        throw std::runtime_error("Image dimensions cannot exceed " + std::to_string(GImage::MAX_SIZE) + "!");

    this->bitmap = new unsigned char[this->width * this->height];

    std::fill_n(this->bitmap, this->width * this->height, 0);
}

GImage::GImage() : GImage(0, 0)
{

}

GImage& GImage::operator=(GImage&& other) noexcept
{
    this->width = other.width;
    this->height = other.height;
    this->bitmap = other.bitmap;
    other.bitmap = nullptr;

    return *this;
}


void GImage::save(const std::filesystem::path &filename) const
{
    std::ofstream output_file(filename, std::ios::binary);

    if (!output_file)
        throw std::runtime_error("Failed to open file: " + filename.string());

    auto err_func = [] (png_structp png_ptr, png_const_charp error_msg) -> void {
        std::cerr << error_msg << std::endl;
    };

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, err_func, err_func);

    if (png == nullptr)
        throw std::runtime_error("Failed to create the PNG write struct.");

    png_rw_ptr write_fn = [] (png_structp png_ptr, png_bytep data, size_t length) -> void {
        auto *output_file = static_cast<std::ofstream *>(png_get_io_ptr(png_ptr));
        output_file->write(reinterpret_cast<char *>(data), length);
    };

    png_flush_ptr flush_fn = [] (png_structp png_ptr) -> void {
        auto *output_file = static_cast<std::ofstream *>(png_get_io_ptr(png_ptr));
        output_file->flush();
    };

    png_set_write_fn(png, &output_file, write_fn, flush_fn);

    png_infop info = png_create_info_struct(png);

    if (info == nullptr)
        throw std::runtime_error("Failed to create the PNG info struct.");

    constexpr png_byte bit_depth = std::numeric_limits<unsigned char>::digits;
    constexpr png_byte color_type = PNG_COLOR_TYPE_GRAY;

    png_set_IHDR(png, info, this->width, this->height, bit_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png, info);

    for (uint32_t i = 0; i < this->height; i++)
        png_write_row(png, reinterpret_cast<png_bytep>(&this->bitmap[i * width]));

    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);
}

uint32_t GImage::getWidth() const
{
    return this->width;
}

uint32_t GImage::getHeight() const
{
    return this->height;
}

unsigned char &GImage::operator[](const uvec2 &xy)
{
    return this->bitmap[xy.x + xy.y * this->width];
}

unsigned char GImage::operator[](const uvec2 &xy) const
{
    return this->bitmap[xy.x + xy.y * this->width];
}

GImage::~GImage()
{
    delete[] this->bitmap;
    this->bitmap = nullptr;
}

GImage::GImage(GImage&& other) noexcept
{
    this->width = other.width;
    this->height = other.height;
    this->bitmap = other.bitmap;
    other.bitmap = nullptr;
}

GImage GImage::resize_bilinear(uint32_t new_width, uint32_t new_height) const
{
    GImage output(new_width, new_height);

    for (uint32_t y = 0; y < new_height; y++)
    {
        for (uint32_t x = 0; x < new_width; x++)
        {
            double sx = static_cast<double>(x) / new_width * this->width;
            double sy = static_cast<double>(y) / new_height * this->height;

            auto cx = static_cast<uint32_t>(std::ceil(sx));
            auto cy = static_cast<uint32_t>(std::ceil(sy));
            auto fx = static_cast<uint32_t>(std::floor(sx));
            auto fy = static_cast<uint32_t>(std::floor(sy));

            double fract_x = sx - std::floor(sx);
            double fract_y = sy - std::floor(sy);

            double sample0 = this->operator[]({fx, fy}) * (1 - fract_x) * (1 - fract_y);
            double sample1 = this->operator[]({cx, fy}) *       fract_x * (1 - fract_y);
            double sample2 = this->operator[]({fx, cy}) * (1 - fract_x) *      fract_y;
            double sample3 = this->operator[]({cx, cy}) *       fract_x *      fract_y;

            auto sample = static_cast<unsigned char>(sample0 + sample1 + sample2 + sample3);

            output[uvec2{x, y}] = sample;
        }
    }

    return output;
}

void GImage::realloc_size(uint32_t new_width, uint32_t new_height)
{
    if (this->width == new_width && this->height == new_height)
        return;

    delete[] this->bitmap;

    this->width = new_width;
    this->height = new_height;

    if (this->width > GImage::MAX_SIZE || this->height > GImage::MAX_SIZE)
        throw std::runtime_error("Image dimensions cannot exceed " + std::to_string(GImage::MAX_SIZE) + "!");

    this->bitmap = new unsigned char[this->width * this->height];

    std::fill_n(this->bitmap, this->width * this->height, 0);
}

GImage GImage::dither(unsigned char threshold) const
{
    GImage output(this->width, this->height);

    constexpr uint32_t border = 1;
    uint32_t err_buf_w = this->width + border * 2;
    uint32_t err_buf_h = this->height + border * 2;
    int *err_buf = new int[err_buf_w * err_buf_h];
    int bias = UCHAR_MAX / 2 - threshold;
    std::fill_n(err_buf, err_buf_w * err_buf_h, bias);

    auto err_buf_add = [=] (uvec2 xy, int error) -> void {
        err_buf[(xy.x + border) + (xy.y + border) * err_buf_w] += error;
    };

    auto err_buf_get = [=] (uvec2 xy) -> int {
        return err_buf[(xy.x + border) + (xy.y + border) * err_buf_w];
    };

    uvec2 pos{};

    for (uint32_t y = 0; y < output.height; y++)
    {
        pos.y = y;

        for (uint32_t x = 0; x < output.width; x++)
        {
            pos.x = x;

            int pixel = (*this)[pos] + err_buf_get(pos);
            unsigned char color = (threshold < pixel) * UCHAR_MAX;
            int error = pixel - color;
            err_buf_add({x + 1, y}, error * 7 / 16);
            err_buf_add({x - 1, y + 1}, error * 3 / 16);
            err_buf_add({x, y + 1}, error * 5 / 16);
            err_buf_add({x + 1, y + 1}, error * 1 / 16);

            output[pos] = color;
        }
    }

    delete[] err_buf;

    return output;
}

GImage GImage::dither_ordered(unsigned char threshold) const
{
    GImage output(this->width, this->height);

    constexpr uint32_t bit_shift = 1u;
    constexpr uint32_t mask_size = 1u << bit_shift;
    constexpr uint32_t mask_pixels = mask_size * mask_size;
    static unsigned int mask[mask_size][mask_size] = {
            { 0, 2 },
            { 3, 1 }
    };

    threshold /= mask_pixels;

    uvec2 pos{};

    for (uint32_t y = 0; y < output.height; y++)
    {
        pos.y = y;

        for (uint32_t x = 0; x < output.width; x++)
        {
            pos.x = x;

            output[pos] = ((this->operator[](pos) * mask[x & bit_shift][y & bit_shift] / mask_pixels) > threshold) * UCHAR_MAX;
        }
    }

    return output;
}

GImage GImage::binary_threshold(unsigned char threshold) const
{
    GImage output(this->width, this->height);

    for (uint32_t i = 0; i < output.width * output.height; i++)
    {
        output.bitmap[i] = (this->bitmap[i] > threshold) * UCHAR_MAX;
    }

    return output;
}

GImage GImage::invert() const
{
    GImage output(this->width, this->height);

    for (uint32_t i = 0; i < output.width * output.height; i++)
    {
        output.bitmap[i] = UCHAR_MAX - this->bitmap[i];
    }

    return output;
}

GImage &GImage::invert_in_place()
{
    for (uint32_t i = 0; i < this->width * this->height; i++)
    {
        this->bitmap[i] = UCHAR_MAX - this->bitmap[i];
    }

    return *this;
}

GImage &GImage::gamma_correct(double correction)
{
    constexpr unsigned char lutMax = UCHAR_MAX;
    unsigned char lut[lutMax + 1];

    for (uint32_t i = 0; i <= lutMax; i++)
    {
        double normalized = i / static_cast<double>(lutMax);
        double corrected = std::pow(normalized, correction);
        uint32_t val = std::round(corrected * lutMax);
        lut[i] = static_cast<unsigned char>(val);
    }

    const uint32_t bufsize = this->width * this->height;

    for (uint32_t i = 0; i < bufsize; i++)
    {
        this->bitmap[i] = lut[this->bitmap[i]];
    }

    return *this;
}

GImage::Histogram GImage::getHistogram() const
{
    Histogram histogram;

    histogram.fill(0);

    for (uint32_t i = 0; i < this->width * this->height; i++)
    {
        unsigned char level = this->bitmap[i];
        histogram[level]++;
    }

    // Bias towards lighter colors
    histogram[0] = 0;

    return histogram;
}


unsigned char GImage::otsu() const
{
    Histogram hs = this->getHistogram();

    uint32_t pixels = 0;
    for (uint32_t i = 0; i < levels; i++)
        pixels += hs[i];

    double sum = 0;
    for (uint32_t i = 0; i < levels; i++)
        sum += static_cast<double>(i) * hs[i];

    double sumB = 0;
    uint32_t wB = 0;
    uint32_t wF;

    double varMax = 0;
    unsigned char threshold = 0;

    for (uint32_t t = 0; t < levels; t++)
    {
        wB += hs[t];

        if (wB == 0)
            continue;

        wF = pixels - wB;

        if (wF == 0)
            break;

        sumB += static_cast<double>(t * hs[t]);

        double mB = sumB / wB;
        double mF = (sum - sumB) / wF;

        double varBetween = static_cast<double>(wB) * static_cast<double>(wF) * (mB - mF) * (mB - mF);

        if (varBetween > varMax)
        {
            varMax = varBetween;
            threshold = t;
        }
    }

    return threshold;
}

unsigned char* GImage::data()
{
    return this->bitmap;
}

const unsigned char* GImage::data() const
{
    return this->bitmap;
}