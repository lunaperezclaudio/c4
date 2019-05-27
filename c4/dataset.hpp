//MIT License
//
//Copyright(c) 2019 Alex Kasitskyi
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "csv.hpp"
#include "lbp.hpp"
#include "json.hpp"
#include "jpeg.hpp"
#include "string.hpp"
#include "range.hpp"
#include "parallel.hpp"
#include "logger.hpp"
#include "matrix_regression.hpp"
#include "image_dumper.hpp"
#include "drawing.hpp"

namespace c4 {
    struct dataset {
        const matrix_dimensions sample_size;
        const bool dump_rects;

        matrix<std::vector<uint8_t>> rx;
        std::vector<float> y;

        dataset(const matrix_dimensions& sample_size, const bool dump_rects = false) : sample_size(sample_size), dump_rects(dump_rects), rx(LBP().calc_dimensions(sample_size)) {}

        template<class TForm>
        static void generate_samples(const matrix_dimensions& sample_size, const matrix<uint8_t>& img, const std::vector<rectangle<int>>& rects, std::vector<matrix<uint8_t>>& pos, std::vector<matrix<uint8_t>>& neg, const float neg_to_pos_ratio, int k, TForm tform) {
            matrix<uint8_t> sample(sample_size);

            int positive = 0;

            for (const auto& r : rects) {
                for (int dx = -k; dx <= k; dx++) {
                    for (int dy = -k; dy <= k; dy++) {
                        rectangle<int> r1(r.x + dx, r.y + dy, r.w, r.h);

                        if (r1.intersect(img.rect()) != r1)
                            continue;

                        scale_bilinear(img.submatrix(r1), sample);
                        pos.push_back(tform(sample));
                        positive++;
                    }
                }
            }

            if (img.height() <= sample.height() || img.width() <= sample.width())
                return;

            fast_rand rnd;
            const int need_neg = int(positive * neg_to_pos_ratio);
            int negatives = 0;
            for(int it = 0; negatives < need_neg && it < need_neg * 10; it++) {
                rectangle<int> r;
                r.y = rnd() % (img.height() - sample_size.height);
                r.x = rnd() % (img.width() - sample_size.width);
                r.h = sample_size.height;
                r.w = sample_size.width;

                double iou_max = 0.;
                for (const auto& t : rects) {
                    iou_max = std::max(iou_max, intersection_over_union(r, t));
                }
                if (iou_max < 0.1) {
                    neg.push_back(tform(img.submatrix(r)));
                    negatives++;
                }
            }
        }

        void load(const std::vector<std::pair<std::string, std::vector<rectangle<int>>>>& data, const int k, const float neg_to_pos_ratio) {
            scoped_timer t("dataset::load");

            enumerable_thread_specific<std::vector<matrix<uint8_t>>> ts_xp;
            enumerable_thread_specific<std::vector<matrix<uint8_t>>> ts_xn;

            parallel_for(data, [&](const std::pair<std::string, std::vector<rectangle<int>>>& r) {
                auto& xp = ts_xp.local();
                auto& xn = ts_xn.local();

                matrix<uint8_t> img;
                read_jpeg(r.first, img);

                generate_samples(sample_size, img, r.second, xp, xn, (k ? 1.3 : 2) * neg_to_pos_ratio, k, LBP());

                if (dump_rects) {
                    for (const auto& rect : r.second) {
                        draw_rect(img, rect, uint8_t{ 255 });
                    }
                    force_dump_image(img, "fd");
                }
            });

            auto counter = [](int a, const std::vector<matrix<uint8_t>>& v) { return a + isize(v); };
            
            int num_p = std::accumulate(ts_xp.begin(), ts_xp.end(), 0, counter);
            int num_n = std::accumulate(ts_xn.begin(), ts_xn.end(), 0, counter);

            auto multipop = [](enumerable_thread_specific<std::vector<matrix<uint8_t>>>& ts_x, int pop_cnt) {
                for (auto& t : ts_x) {
                    while (!t.empty() && pop_cnt > 0) {
                        t.pop_back();
                        pop_cnt--;
                    }

                    t.shrink_to_fit();
                }
            };

            multipop(ts_xp, num_p - int(num_n / neg_to_pos_ratio + 0.5f));
            multipop(ts_xn, num_n - int(num_p * neg_to_pos_ratio + 0.5f));

            const int capacity = std::accumulate(ts_xp.begin(), ts_xp.end(), 0, counter) + std::accumulate(ts_xn.begin(), ts_xn.end(), 0, counter);

            y.reserve(capacity);

            for (auto& row : rx) {
                for (auto& v : row) {
                    v.reserve(capacity);
                }
            }

            for (auto& t : ts_xp) {
                matrix_regression<>::push_back_repack(t, rx);
                y.resize(y.size() + t.size(), 1.f);
                t.clear();
                t.shrink_to_fit();
            }

            for (auto& t : ts_xn) {
                matrix_regression<>::push_back_repack(t, rx);
                y.resize(y.size() + t.size(), 0.f);
                t.clear();
                t.shrink_to_fit();
            }
        }

        void load_dlib(const std::string& labels_filepath, const int k = 0, const int sample = 1) {
            json data_json;

            std::string root = "C:/portraits-data/ibug_300W_large_face_landmark_dataset/";
            std::ifstream train_data_fin(root + labels_filepath);

            train_data_fin >> data_json;

            std::vector<std::pair<std::string, std::vector<rectangle<int>>>> data;

            for (int i : range(data_json["images"].size())) {
                const auto& image_info = data_json["images"][i];

                const std::string filename = image_info["filename"];

                if (!ends_with(to_lower(filename), ".jpg") || i % sample)
                    continue;

                std::vector<rectangle<int>> rects;

                for (const auto& box : image_info["boxes"]) {
                    rectangle<int> r(box["left"], box["top"], box["width"], box["height"]);
                    rects.push_back(r);
                }

                data.emplace_back(root + filename, rects);
            }

            load(data, k, 1.f);
        }
        
        [[deprecated]]
        void load_vggface2(const std::string& root, const std::string& labels_filepath, const int k = 0, const int sample = 1) {
            std::vector<std::pair<std::string, std::vector<rectangle<int>>>> data;

            std::ifstream fin(labels_filepath);

            csv csv;
            csv.read(fin);
            for (int i : range(1, isize(csv.data))) {
                if (i % sample)
                    continue;

                const auto& row = csv.data[i];

                data.emplace_back(root + row[0] + ".jpg", std::vector<rectangle<int>>{ rectangle<int>(stoi(row[1]), stoi(row[2]), stoi(row[3]), stoi(row[4])) });
            }

            load(data, k, 1.f);
        }

        void load_vggface2_landmark(const std::string& root, const std::string& labels_filepath, const float face_scale, const int k = 0, const int sample = 1) {
            std::vector<std::pair<std::string, std::vector<rectangle<int>>>> data;

            std::ifstream fin(labels_filepath);

            csv csv;
            csv.read(fin);
            for (int i : range(1, isize(csv.data))) {
                if (i % sample)
                    continue;

                const auto& row = csv.data[i];
                std::vector<point<float>> v(5);
                for (int j : range(v)) {
                    v[j].x = string_to<float>(row[2 * j + 1]);
                    v[j].y = string_to<float>(row[2 * j + 2]);
                }

                const point<float> center = std::accumulate(v.begin(), v.end(), point<float>()) * (1.f / v.size());

                float max_d = 0.f;
                for (auto p : v) {
                    max_d = std::max(max_d, dist(p, center));
                }

                const float half_side = max_d * face_scale;
                const int side = int(2 * half_side + 0.5f);

                rectangle<int> r(int(center.x - half_side + 0.5f), int(center.y - half_side + 0.5f), side, side);

                if (dump_rects) {
                    matrix<uint8_t> img;
                    read_jpeg(root + row[0] + ".jpg", img);

                    draw_rect(img, r, uint8_t(255));

                    for (int j : range(v)) {
                        draw_point(img, int(v[j].y), int(v[j].x), uint8_t(255), 10);
                    }

                    force_dump_image(img, "lm");
                }

                data.emplace_back(root + row[0] + ".jpg", std::vector<rectangle<int>>{ r });
            }

            load(data, k, 1.f);
        }
    };
}; // namespace c4
