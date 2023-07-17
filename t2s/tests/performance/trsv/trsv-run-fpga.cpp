/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/
// The header file generated by gemv.cpp
#include "trsv-interface.h"

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"

// Roofline utilities
#include "Roofline.h"

// The only header file needed for including T2S.
#include "HalideBuffer.h"

// For printing output
#include <stdio.h>
#include <iostream>

// For validation of results.
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = II * I;
    assert(TOTAL_I == K);
    Halide::Runtime::Buffer<float> a(TOTAL_I+K, K), x(K);

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t k = 0; k < K; k++) {
            a(i, k) = (i < k) ? 0 : (random()%10)+1;
            // printf("%.0lf ", a(i, k));
        }
    }

    for (size_t k = 0; k < K; k++) {
        x(k) = random()%10;
    }

    Halide::Runtime::Buffer<float> b(TOTAL_I);
    for (size_t i = 0; i < TOTAL_I; i++) {
        float psum = 0.0f;
        for (size_t k = 0; k < K; k++) {
            psum += a(i, k) * x(k);
        }
        b(i) = psum;
    }

    Halide::Runtime::Buffer<float> out_x(K);
    trsv(a, b, out_x);

#ifdef TINY
    for (int k = 0; k < K; k++) {
        // printf("%lf, %lf\n", out_x(k), x(k));
        assert(fabs(out_x(k) - x(k)) <= 0.005*fabs(x(k)));
    }
#else
    // Report performance. DSPs, FMax and ExecTime are automatically figured out from the static analysis
    // during FPGA synthesis and and the dynamic profile during the FGPA execution.
    float mem_bandwidth = 34; // pac_a10 on DevCloud has 34GB/s memory bandwidth
    float compute_roof = 2 * DSPs() * FMax();
    float number_ops = 2 * (float)TOTAL_I * (float)K; // Total operations (GFLOP for GEMV), independent of designs
    float number_bytes = (float)TOTAL_I * (float)K * 4 +
                         (float)K * 4 +
                         (float)TOTAL_I * 4;
    float exec_time= ExecTime();
    roofline(mem_bandwidth, compute_roof, number_ops, number_bytes,exec_time);
    if (fopen("roofline.png", "r") == NULL) {
        cout << "Failed to draw roofline!\n";
        return 1;
    }
#endif

    printf("Success\n");
    return 0;
}
