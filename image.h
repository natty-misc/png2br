//
// Created by michal on 05.09.20.
//

#ifndef PNG2BR_IMAGE_H
#define PNG2BR_IMAGE_H

#include "util.h"

#include <filesystem>
#include <string>

#include <limits>

class GImage
{
    public:
        static constexpr uint32_t MAX_SIZE = 16384;
        static constexpr uint32_t levels = std::numeric_limits<unsigned char>::max() + 1;
        typedef std::array<uint32_t, levels> Histogram;

        explicit GImage(const std::filesystem::path &filename);
        GImage(uint32_t width, uint32_t height);
        GImage(GImage&& other) noexcept;
        GImage();

        GImage& operator=(GImage&& other) noexcept;

        void save(const std::filesystem::path &filename) const;

        [[nodiscard]] uint32_t getWidth() const;
        [[nodiscard]] uint32_t getHeight() const;
        unsigned char &operator[](const uvec2 &xy);
        [[nodiscard]] unsigned char operator[](const uvec2 &xy) const;
        [[nodiscard]] Histogram getHistogram() const;
        [[nodiscard]] unsigned char otsu() const;
        [[nodiscard]] GImage invert() const;
        GImage &invert_in_place();
        GImage &gamma_correct(double correction);
        [[nodiscard]] GImage resize_bilinear(uint32_t new_width, uint32_t new_height) const;
        void realloc_size(uint32_t new_width, uint32_t new_height);
        [[nodiscard]] GImage dither(unsigned char threshold) const;
        [[nodiscard]] GImage dither_ordered(unsigned char threshold) const;
        [[nodiscard]] GImage binary_threshold(unsigned char threshold) const;
        [[nodiscard]] unsigned char* data();
        [[nodiscard]] const unsigned char* data() const;
        ~GImage();

    private:
        unsigned char *bitmap;
        uint32_t width;
        uint32_t height;
};


#endif //PNG2BR_IMAGE_H
