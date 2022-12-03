#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#endif

#include "image.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Argument needed: filename." << std::endl;
        return EXIT_FAILURE;
    }


#ifdef _WIN32
    std::cout << "This program doesn't work with CMD or PowerShell windows as they don't support unicode characters. 😡" << std::endl;
    const unsigned int initial_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    fflush(stdout);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    try
    {
        std::filesystem::path file(argv[1]);

        if (!std::filesystem::is_regular_file(file))
        {
            throw std::runtime_error(file.string() + " is not a valid file!");
        }

        GImage img{file};

        std::cout << "Image file: " << file << std::endl;
        std::cout << "  Width: " << img.getWidth() << std::endl;
        std::cout << "  Height: " << img.getHeight() << std::endl;

        uint32_t resized_width = 0;
        uint32_t resized_height = 0;

        do
        {
            if (!std::cin)
            {
                std::cin.clear();
                std::cin.ignore();
            }

            std::cout << "  Resized width (enter a number): ";
            std::cin >> resized_width;
        }
        while (!std::cin || resized_width == 0);

        do
        {
            if (!std::cin)
            {
                std::cin.clear();
                std::cin.ignore();
            }

            std::cout << "  Resized height (enter a number): ";
            std::cin >> resized_height;
        }
        while (!std::cin || resized_height == 0);

        constexpr uint32_t rescale_x = 2;
        constexpr uint32_t rescale_y = 4;

        GImage resized_copy = img.resize_bilinear(resized_width * rescale_x, resized_height * rescale_y);
        unsigned char threshold = img.otsu();
        GImage resized_img = resized_copy.binary_threshold(threshold);

        const auto& target = resized_img;

        std::cout << "  Actual width: " << target.getWidth() << std::endl;
        std::cout << "  Actual height: " << target.getHeight() << std::endl;
        std::cout << "  Threshold: " << static_cast<unsigned int>(threshold) << std::endl;

        constexpr int aspect0 = 12;
        constexpr int aspect1 = 24;

        for (uint32_t y = 0; y < target.getHeight() / rescale_y * aspect0 / aspect1; y++)
        {
            uint32_t by = y * aspect1 / aspect0 * rescale_y;

            for (uint32_t x = 0; x < target.getWidth() / rescale_x; x++)
            {
                uint32_t bx = x * rescale_x;

                uint32_t pattern = 0;

                pattern += 0x01u & target[{bx, by}];
                pattern += 0x02u & target[{bx, by + 1}];
                pattern += 0x04u & target[{bx, by + 2}];
                pattern += 0x08u & target[{bx + 1, by}];
                pattern += 0x10u & target[{bx + 1, by + 1}];
                pattern += 0x20u & target[{bx + 1, by + 2}];
                pattern += 0x40u & target[{bx, by + 3}];
                pattern += 0x80u & target[{bx + 1, by + 3}];

                if (pattern == 0)
                    pattern = 1;

                pattern |= 0x2800u;

                // Some UTF-8 magic
                char c[4] = { 0 };
                c[0] = static_cast<char>((pattern >> 12u) + 0xE0u);
                c[1] = static_cast<char>(((pattern >> 6u) & 0x3Fu) + 0x80u);
                c[2] = static_cast<char>((pattern & 0x3Fu) + 0x80u);

                std::cout << c;
            }

            std::cout << std::endl;
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

#ifdef _WIN32
    SetConsoleOutputCP(initial_cp);
#endif

    return 0;
}
