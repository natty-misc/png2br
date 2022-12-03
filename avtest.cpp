#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
#include <windows.h>
#elif defined(POSIX) or defined(__linux__)
#include <unistd.h>
#else
#error "Unsupported platform! :("
#endif

#if not defined(_WIN32) or defined(__MINGW32__) or defined(__MINGW64__)
#define ENABLE_BRAILLE 1
#endif

#define BRAILLE_WORKAROUND 0
#define ENABLE_BRAILLE 1
#define USE_COLOR 1

#include <array>
#include <condition_variable>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>

#include "image.h"
#include "videodecoder.h"

static constexpr uint32_t rescale_x = 2;
static constexpr uint32_t rescale_y = 4;

void print_img(const GImage& img)
{

#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(console, &mode);
    SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::stringstream strFrame;
    strFrame << "\033[2;0H";

#if USE_COLOR
    uint32_t prevVal = 255;
    uint32_t colorChanges = 0;
#endif

    for (uint32_t y = 0; y < img.getHeight(); y += rescale_y)
    {
        for (uint32_t x = 0; x < img.getWidth(); x += rescale_x)
        {

#if USE_COLOR
            uint32_t value = 0;

            value += img[{x, y}];
            value += img[{x, y + 1}];
            value += img[{x, y + 2}];
            value += img[{x + 1, y}];
            value += img[{x + 1, y + 1}];
            value += img[{x + 1, y + 2}];
            value += img[{x, y + 3}];
            value += img[{x + 1, y + 3}];

            auto avgVal = static_cast<uint32_t>(value / 8.0);

            constexpr uint32_t levels = 8;

            avgVal /= levels;
            avgVal *= levels;


            if (prevVal != avgVal)
            {
                strFrame << "\033[38;2;" << avgVal << ";" << avgVal << ";" << avgVal << "m";
                prevVal = avgVal;
                colorChanges++;
            }
#endif

#if ENABLE_BRAILLE
            uint32_t pattern = 0;
            pattern += 0x01u & img[{x, y}];
            pattern += 0x02u & img[{x, y + 1}];
            pattern += 0x04u & img[{x, y + 2}];
            pattern += 0x08u & img[{x + 1, y}];
            pattern += 0x10u & img[{x + 1, y + 1}];
            pattern += 0x20u & img[{x + 1, y + 2}];
            pattern += 0x40u & img[{x, y + 3}];
            pattern += 0x80u & img[{x + 1, y + 3}];

#if BRAILLE_WORKAROUND
            if (pattern == 0)
                pattern = 0x01u;
#endif
            char c[4] = {0};

            pattern |= 0x2800u;

            // Some UTF-8 magic
            c[0] = static_cast<char>((pattern >> 12u) + 0xE0u);
            c[1] = static_cast<char>(((pattern >> 6u) & 0x3Fu) + 0x80u);
            c[2] = static_cast<char>((pattern & 0x3Fu) + 0x80u);

#elif USE_COLOR
            // Fall back to ASCII
            char c = '@';
#else
            char c = (avgVal > UCHAR_MAX / 2) * '@' + (avgVal <= UCHAR_MAX / 2) * ' ';
#endif

            strFrame << c;
        }

        strFrame << '\n';
    }


#if USE_COLOR
    strFrame << "\033[2;0H";
    strFrame << "\033[38;2;20;200;255mColor changes: ";
    uint32_t j = 0;
    for (uint32_t i = 0; i < colorChanges; i += 100, j++)
        strFrame << "#";
    strFrame << "\033[38;2;0;0;0m";
#endif

    std::cout << strFrame.str();

    std::cout << std::flush;
}

int main(int argc, char** argv)
{
    std::string program = argv[0];
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.size() != 1)
    {
        std::cerr << "Usage: " << program << " <filename>";
        return EXIT_SUCCESS;
    }

    std::filesystem::path file = args[0];

    VideoDecoder decoder(file);

    struct QueueItem
    {
        GImage* frame;
        double pts;
        double timeBase;
        int currentBufferIdx;
    };

    std::atomic_bool decodeFinished;
    std::mutex queueMutex;
    std::queue<QueueItem> queuedBuffers;

    std::mutex queueNotFullMutex;
    std::condition_variable queueNotFull;

    std::mutex queueNotEmptyMutex;
    std::condition_variable queueNotEmpty;

    constexpr int nSwapBuffers = 8;
    std::array<GImage, nSwapBuffers> frameBuffers;

    std::thread decodeThread([&] {
        int frameBufferIdx = 0;
        GImage img;

        while (true)
        {
            std::unique_lock<std::mutex> lock(queueNotFullMutex);
            queueNotFull.wait(lock, [&queuedBuffers] { return queuedBuffers.size() < nSwapBuffers - 1; });

            std::unique_lock<std::mutex> queueLock(queueMutex);

            if (!decoder.decodeFrame(img))
                break;

            if (decoder.hasFrame())
            {
                std::uint32_t width = 96;
                std::uint32_t height = 60 / 2;

                width *= rescale_x;
                height *= rescale_y;

                static int i = 0;

                img.gamma_correct(2.2);
                int threshold = img.otsu();
                frameBuffers[frameBufferIdx] = std::move(img.resize_bilinear(640, 360).dither(threshold));

                queuedBuffers.push({
                   &frameBuffers[frameBufferIdx],
                   decoder.getPTS(),
                   decoder.getTimeBase(),
                   frameBufferIdx
                });

                queueNotEmpty.notify_all();

                frameBufferIdx++;
                frameBufferIdx %= nSwapBuffers;
            }
        }

        decodeFinished = true;
    });

    auto startTime = std::chrono::high_resolution_clock::now();

    while (true)
    {
        std::unique_lock<std::mutex> lock(queueNotEmptyMutex);
        queueNotEmpty.wait(lock, [&] { return !queuedBuffers.empty() || decodeFinished; });
        std::unique_lock<std::mutex> queueLock(queueMutex);

        if (decodeFinished && queuedBuffers.empty())
            break;

        auto& item = queuedBuffers.front();

        double frameTimestamp = item.pts * item.timeBase;

        auto frameStart = std::chrono::steady_clock::now();
        print_img(*item.frame);
        auto frameEnd = std::chrono::steady_clock::now();

        std::stringstream infoOSD;

        static int frameNumber = 0;
        char frameNum[32];
        snprintf(frameNum, sizeof(frameNum), "Frame number: %d", frameNumber++);
        char ptsNum[32];
        snprintf(ptsNum, sizeof(ptsNum), "PTS: %.2lf", item.pts);
        char secondsNum[32];
        snprintf(secondsNum, sizeof(secondsNum), "Seconds: %.2lf", frameTimestamp);
        char realtime[32];
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
        snprintf(realtime, sizeof(realtime), "Real time: %.2lf", static_cast<double>(timeDiff) / 1000000.0);
        char timeBaseStr[32];
        snprintf(timeBaseStr, sizeof(timeBaseStr), "1/Time base: %g", 1 / item.timeBase);
        char bufCount[32];
        snprintf(bufCount, sizeof(bufCount), "Buffer: %lu", queuedBuffers.size());
        char curBuf[32];
        snprintf(curBuf, sizeof(curBuf), "Current buffer: %d", item.currentBufferIdx);
        auto printDuration = frameEnd - frameStart;
        long frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(printDuration).count();
        char frameTimeStr[32];
        snprintf(frameTimeStr, sizeof(frameTimeStr), "Frame time: %ldms", frameTime);

        infoOSD << "\033[1;1H";
        infoOSD << "\033[38;2;20;200;255m";
        infoOSD << std::setw(32) << std::left << frameNum
                << std::setw(32) << std::left << ptsNum
                << std::setw(32) << std::left << secondsNum
                << std::setw(32) << std::left << realtime
                << std::setw(32) << std::left << timeBaseStr
                << std::setw(24) << std::left << bufCount
                << std::setw(24) << std::left << curBuf
                << std::setw(24) << std::left << frameTimeStr;
        infoOSD << "\033[38;2;255;255;255m";
        std::cout << infoOSD.str() << std::flush;

        queuedBuffers.pop();
        queueLock.unlock();
        lock.unlock();
        queueNotFull.notify_all();

        long expectedIdleUs = static_cast<long>(frameTimestamp * 1000000.0 - timeDiff) - frameTime * 1000;
        expectedIdleUs = std::max(expectedIdleUs, 1000L);
        std::this_thread::sleep_for(std::chrono::microseconds(expectedIdleUs));
    }

    decodeThread.join();
}
