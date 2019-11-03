#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <algorithm>
#include <cmath>

#include <libusb-1.0/libusb.h>

#pragma pack(push, 1)
struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
#pragma pack(pop)
static_assert(sizeof(RGB) == 3);

// Maps a value from one range to another
// taken from Processing: https://stackoverflow.com/a/17135426
static inline double map(double n, double min1, double max1, double min2, double max2) {
    return min2 + (max2 - min2) * ((n - min1) / (max1 - min1));
}

template<typename T, size_t N>
struct MakeArray {
    T arr[N];
    static const size_t size = N;

    template<typename F>
    constexpr MakeArray(F f) : arr() {
        f(N, arr);
    }
};

template<size_t N>
struct KeyScales {
    struct CellScale {
        size_t x = 0;
        size_t y = 0;
        float scale = 1.0f;
    };

    size_t num_cells = 0;
    CellScale cell_scales[N];
};

class CMMKProM {
  public:
    static const int usb_interface = 1;
    static const unsigned char usb_endpoint_out = 4 | LIBUSB_ENDPOINT_OUT;
    static const unsigned char usb_endpoint_in = 3 | LIBUSB_ENDPOINT_IN;

    static const size_t key_map_cols = 19;
    static const size_t key_map_rows = 6;
    using led_matrix = RGB[key_map_rows][key_map_cols];
    // mapping of matrix positions to index in data stream
    static constexpr ssize_t key_map[key_map_rows][key_map_cols] = {
        /*
         ESC  F1   F2   F3   F4        F5   F6   F7   F8        F9   F10  F11  F12                    */
        {  0,   8,  16,  24,  32, -1 ,  40,  48,  56,  64, -1 ,  72,  80,  88,  96, -1 , -1 , -1 , -1 },
        /*
         ~     1    2    3    4    5    6    7    8    9    0   -_   =+        BCK  NUM  KP/  KP*  KP- */
        {  1,   9,  17,  25,  33,  41,  49,  57,  65,  73,  81,  89,  97, -1 , 104, 109,  70,  63,  71},
        /*
         TAB   Q    W    E    R    T    Y    U    I    O    P    [    ]        \|   KP7  KP8  KP9  KP+ */
        {  2,  10,  18,  26,  34,  42,  50,  58,  66,  74,  82,  90,  98, -1 , 106,  54,  62,  55,  47},
        /*
         CAP   A    S    D    F    G    H    J    K    L   ;:   '"             ENT  KP4  KP5  KP6      */
        {  3,  11,  19,  27,  35,  43,  51,  59,  67,  75,  83,  91, -1 , -1 , 107,  38,  46,  31, -1 },
        /*
         LSH        Z    X    C    V    B    N    M   ,<   .>   /?             RSH  KP1  KP2  KP3  KPE */
        {  4, -1 ,  20,  28,  36,  44,  52,  60,  68,  76,  84,  92, -1 , -1 , 108,  22,  30,  23,  15},
        /*
         LCL  LWN       LAL            SPC                 RAL  RWN   FN       RCL   KP0  K00  KP.     */
        {  5,  13, -1 ,  21, -1 , -1 ,  53, -1 , -1 , -1 ,  77,  85,  93, -1 , 101,    6,  14,  7, -1 },
    };
    static const size_t num_keys = []() constexpr -> size_t {
        const size_t max_keys = 256;
        bool keys[max_keys]{};
        for (size_t i = 0; i < key_map_cols * key_map_rows; i++) {
            const ssize_t key = key_map[i / key_map_cols][i % key_map_cols];
            if (key < 0)
                continue;
            if (static_cast<size_t>(key) > max_keys)
                throw "Key value too large, please adjust max_keys in this function";
            keys[key] = true;
        }
        size_t count = 0;
        for (size_t i = 0; i < max_keys; i++) {
            if (keys[i])
                count++;
        }
        return count;
    }();
    static constexpr auto key_ids = MakeArray<ssize_t, num_keys>(
        [](size_t size, auto arr) constexpr -> void {
            size_t j = 0;
            for (size_t i = 0; i < key_map_cols * key_map_rows; i++) {
                const ssize_t key = key_map[i / key_map_cols][i % key_map_cols];
                if (key < 0)
                    continue;
                arr[j] = key;
                j++;
            }
            if (j != size)
                throw "All key ids were not filled in";
        }
    );

    static const size_t big_key_map_rows = key_map_rows;
    static const size_t big_key_map_cols = key_map_cols * 4;
    // Bigger version of the above keymap that takes into account the physical positions of the keys
    // The keys on the board come in quarter sizes (eg the CTRL keys are 1.25 units wide)
    static constexpr ssize_t big_key_map[big_key_map_rows][big_key_map_cols] = {
        /*
         |       ESC      |                      |       F1       |  |       F2       |  |       F3       |  |       F4       |            |       F5       |  |       F6       |  |       F7       |  |       F8       |            |       F9       |  |       F10      |  |       F11      |  |       F12      |                                                                                 */
        {  0,   0,   0,   0,  -1,  -1,  -1,  -1,   8,   8,   8,   8,  16,  16,  16,  16,  24,  24,  24,  24,  32,  32,  32,  32,  -1,  -1,  40,  40,  40,  40,  48,  48,  48,  48,  56,  56,  56,  56,  64,  64,  64,  64,  -1,  -1,  72,  72,  72,  72,  80,  80,  80,  80,  88,  88,  88,  88,  96,  96,  96,  96,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1},
        /*
         |        ~       |  |        1       |  |        2       |  |        3       |  |        4       |  |        5       |  |        6       |  |        7       |  |        8       |  |        9       |  |        0       |  |       -_       |  |       =+       |  |              BACKSPACE             |  |     NUMLOCK    |  |      KP /      |  |      KP *      |  |      KP -      | */
        {  1,   1,   1,   1,   9,   9,   9,   9,  17,  17,  17,  17,  25,  25,  25,  25,  33,  33,  33,  33,  41,  41,  41,  41,  49,  49,  49,  49,  57,  57,  57,  57,  65,  65,  65,  65,  73,  73,  73,  73,  81,  81,  81,  81,  89,  89,  89,  89,  97,  97,  97,  97, 104, 104, 104, 104, 104, 104, 104, 104, 109, 109, 109, 109,  70,  70,  70,  70,  63,  63,  63,  63,  71,  71,  71,  71},
        /*
         |            TAB           |  |        Q       |  |        W       |  |        E       |  |        R       |  |        T       |  |        Y       |  |        U       |  |        I       |  |        O       |  |        P       |  |        [       |  |        ]       |  |            \|            |  |      KP 7      |  |      KP 8      |  |      KP 9      |  |      KP +      | */
        {  2,   2,   2,   2,   2,   2,  10,  10,  10,  10,  18,  18,  18,  18,  26,  26,  26,  26,  34,  34,  34,  34,  42,  42,  42,  42,  50,  50,  50,  50,  58,  58,  58,  58,  66,  66,  66,  66,  74,  74,  74,  74,  82,  82,  82,  82,  90,  90,  90,  90,  98,  98,  98,  98, 106, 106, 106, 106, 106, 106,  54,  54,  54,  54,  62,  62,  62,  62,  55,  55,  55,  55,  47,  47,  47,  47},
        /*
         |            CAPSLOCK           |  |        A       |  |        S       |  |        D       |  |        F       |  |        G       |  |        H       |  |        J       |  |        K       |  |        L       |  |       ;:       |  |       '"       |  |                  ENTER                  |  |      KP 4      |  |      KP 5      |  |      KP 6      |  |      KP +      | */
        {  3,   3,   3,   3,   3,   3,   3,  11,  11,  11,  11,  19,  19,  19,  19,  27,  27,  27,  27,  35,  35,  35,  35,  43,  43,  43,  43,  51,  51,  51,  51,  59,  59,  59,  59,  67,  67,  67,  67,  75,  75,  75,  75,  83,  83,  83,  83,  91,  91,  91,  91, 107, 107, 107, 107, 107, 107, 107, 107, 107,  38,  38,  38,  38,  46,  46,  46,  46,  31,  31,  31,  31,  47,  47,  47,  47},
        /*
         |                  LSHIFT                 |  |        Z       |  |        X       |  |        C       |  |        V       |  |        B       |  |        N       |  |        M       |  |       ,<       |  |       .>       |  |       /?       |  |                       RSHIFT                      |  |      KP 1      |  |      KP 2      |  |      KP 3      |  |    KP ENTER    | */
        {  4,   4,   4,   4,   4,   4,   4,   4,   4,  20,  20,  20,  20,  28,  28,  28,  28,  36,  36,  36,  36,  44,  44,  44,  44,  52,  52,  52,  52,  60,  60,  60,  60,  68,  68,  68,  68,  76,  76,  76,  76,  84,  84,  84,  84,  92,  92,  92,  92, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,  22,  22,  22,  22,  30,  30,  30,  30,  23,  23,  23,  23,  15,  15,  15,  15},
        /*
         |        LCTRL        |  |         LWIN        |  |         LALT        |  |                                                          SPACE                                                          |  |         RALT        |  |         RWIN        |  |          FN         |  |        RCTRL        |  |   KP 0  LEFT   |  |      KP 00     |  |      KP .      |  |    KP ENTER    | */
        {  5,   5,   5,   5,   5,  13,  13,  13,  13,  13,  21,  21,  21,  21,  21,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  53,  77,  77,  77,  77,  77,  85,  85,  85,  85,  85,  93,  93,  93,  93,  93, 101, 101, 101, 101, 101,   6,   6,   6,   6,  14,  14,  14,  14,   7,   7,   7,   7,  15,  15,  15,  15},
    };

    static const size_t max_key_cells = []() constexpr -> size_t {
        const size_t y_scale = big_key_map_rows / key_map_rows;
        const size_t x_scale = big_key_map_cols / key_map_cols;

        size_t max_count = 0;
        for (size_t i = 0; i < num_keys; i++) {
            const ssize_t key = key_ids.arr[i];
            bool covered_cells[key_map_rows][key_map_cols]{};

            // Set each cell that this key covers to true
            for (size_t y = 0; y < big_key_map_rows; y++)
                for (size_t x = 0; x < big_key_map_cols; x++)
                    if (big_key_map[y][x] == key)
                        covered_cells[y / y_scale][x / x_scale] = true;

            // Count the number of cells that the key covers
            size_t count = 0;
            for (size_t y = 0; y < key_map_rows; y++)
                for (size_t x = 0; x < key_map_cols; x++)
                    if (covered_cells[y][x])
                        count++;

            max_count = std::max(max_count, count);
        }
        return max_count;
    }();

    static constexpr auto key_scales = MakeArray<KeyScales<max_key_cells>, num_keys>(
        [](size_t size, auto arr) constexpr -> void {
            const size_t y_scale = big_key_map_rows / key_map_rows;
            const size_t x_scale = big_key_map_cols / key_map_cols;

            for (size_t i = 0; i < size; i++) {
                const ssize_t key = key_ids.arr[i];

                // compute how many times this key lands on each cell
                int total_hits = 0;
                int cell_hits[key_map_rows][key_map_cols]{};
                for (size_t y = 0; y < big_key_map_rows; y++) {
                    for (size_t x = 0; x < big_key_map_cols; x++) {
                        if (big_key_map[y][x] != key)
                            continue;
                        cell_hits[y / y_scale][x / x_scale]++;
                        total_hits++;
                    }
                }

                // store the fraction of each cell that this key lands on
                size_t &num_cells = arr[i].num_cells;
                for (size_t y = 0; y < key_map_rows; y++) {
                    for (size_t x = 0; x < key_map_cols; x++) {
                        if (cell_hits[y][x] <= 0)
                            continue;
                        auto &cell = arr[i].cell_scales[num_cells];
                        cell.x = x;
                        cell.y = y;
                        cell.scale = (float)cell_hits[y][x] / (float)total_hits;
                        num_cells++;
                    }
                }
            }
        }
    );

  private:
    libusb_context *ctx = nullptr;
    libusb_device_handle *dev = nullptr;

    void throw_if_err(int ret, std::string msg) {
        if (ret == 0)
            return;
        msg += "; ret = " + std::to_string(ret);
        throw std::runtime_error(msg.c_str());
    }

    std::pair<int, int> send_command(
        uint8_t *data,
        uint8_t *recv_data,
        size_t size
    ) {
        std::pair<int, int> actual;

        throw_if_err(
            libusb_interrupt_transfer(
                dev,
                usb_endpoint_out,
                data,
                size,
                &actual.first,
                100
            ),
            "Failed to send data"
        );

        throw_if_err(
            libusb_interrupt_transfer(
                dev,
                usb_endpoint_in,
                data,
                size,
                &actual.second,
                100
            ),
            "Failed to receive data"
        );

        return actual;
    }

    int recv_data(uint8_t *data, size_t data_size) {
        int actual;
        return actual;
    }

    struct SetLedsData {
        uint8_t v1 = 0xc0;
        uint8_t v2 = 0x02;
        uint8_t v3 = 0xFF;
        uint8_t v4 = 0x00;
        RGB leds[16]{};
        uint8_t padding[12]{};

        SetLedsData(uint8_t index) {
            v3 = index * 2;
        }
    };
    static_assert(sizeof(SetLedsData) == 64);


  public:
    CMMKProM() {
        throw_if_err(
            libusb_init(&ctx),
            "Failed to init libusb"
        );

        dev = libusb_open_device_with_vid_pid(ctx, 0x2516, 0x0048);
        if (!dev)
            throw std::runtime_error("Failed to open device");

        if (libusb_kernel_driver_active(dev, usb_interface)) {
            throw_if_err(
                libusb_detach_kernel_driver(dev,  usb_interface),
                "Failed to detach kernel driver"
            );
        }

        throw_if_err(
            libusb_claim_interface(dev, usb_interface),
            "Failed to claim interface"
        );

        enable_led_control();
    }

    ~CMMKProM() {
        libusb_release_interface(dev, usb_interface);
        libusb_attach_kernel_driver(dev, usb_interface);
        libusb_close(dev);
        libusb_exit(ctx);
    }

    void enable_led_control() {
        uint8_t data[64] = {0x41, 2};
        send_command(data, data, sizeof(data));
    }

    void set_leds(led_matrix matrix) {
        RGB linear_data[256]{};

        for (size_t y = 0; y < key_map_rows; y++) {
            for (size_t x = 0; x < key_map_cols; x++) {
                ssize_t i = key_map[y][x];
                if (i < 0)
                    continue;
                linear_data[i] = matrix[y][x];
            }
        }

        size_t lin_i = 0;
        for (size_t i = 0; i < 7; i++) {
            uint8_t recv_data[64];
            SetLedsData data(i);

            for (size_t j = 0; j < 16; j++) {
                data.leds[j] = linear_data[lin_i];
                lin_i++;
            }

            send_command(
                reinterpret_cast<uint8_t *>(&data),
                recv_data,
                sizeof(SetLedsData)
            );
        }
    }

    void set_leds_smooth(led_matrix matrix, bool use_rgb = false) {
        RGB linear_data[256]{};

        for (size_t i = 0; i < key_scales.size; i++) {
            const auto& key_scale = key_scales.arr[i];
            const auto key = key_ids.arr[i];

            float r = 0, g = 0, b = 0;
            for (size_t j = 0; j < key_scale.num_cells; j++) {
                const auto& cell = key_scale.cell_scales[j];
                const auto& rgb = matrix[cell.y][cell.x];
                r += (float)rgb.r * cell.scale;
                if (use_rgb) {
                    g += (float)rgb.g * cell.scale;
                    b += (float)rgb.b * cell.scale;
                }
            }
            r = std::clamp(r, 0.0f, 255.0f);
            if (use_rgb) {
                g = std::clamp(g, 0.0f, 255.0f);
                b = std::clamp(b, 0.0f, 255.0f);
            }
            linear_data[key] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
        }

        size_t lin_i = 0;
        for (size_t i = 0; i < 7; i++) {
            uint8_t recv_data[64];
            SetLedsData data(i);

            for (size_t j = 0; j < 16; j++) {
                data.leds[j] = linear_data[lin_i];
                lin_i++;
            }

            send_command(
                reinterpret_cast<uint8_t *>(&data),
                recv_data,
                sizeof(SetLedsData)
            );
        }
    }

    void do_thing() {
        using clock = std::chrono::steady_clock;
        auto start = clock::now();
        while (1) {
            std::chrono::duration<double> elapsed = clock::now() - start;
            double elapsed_sec = elapsed.count() * 2.0;
            led_matrix matrix{};

            for (size_t x = 0; x < key_map_cols; x++) {
                double dist = fabs((double)x - fmod(elapsed_sec, (double)key_map_cols));
                dist = std::min(dist, (double)key_map_cols - dist);
                double val = map(dist, 0.0, 1.3, 255.0, 0.0);
                val = std::clamp(val, 20.0, 255.0);
                for (size_t y = 0; y < key_map_rows; y++) {
                    matrix[y][x].r = (uint8_t)val;
                }
            }
            set_leds_smooth(matrix);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
};