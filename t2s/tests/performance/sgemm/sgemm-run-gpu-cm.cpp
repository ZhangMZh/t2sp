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
#define GPU     1

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"

#define K           64
#define J           128
#define I           32

#include <math.h>
#include <assert.h>
#include "l0_rt_helpers.h"

    float *a = (float*)malloc(sizeof(float) * SIZE_A);
    for (int i = 0; i < SIZE_A; ++i) {
        a[i] = rand();
    }
    float *b = (float*)malloc(sizeof(float) * SIZE_B);
    for (int i = 0; i < SIZE_B; ++i) {
        b[i] = rand();
    }

    ze_image_format_t fmt = {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT};
    auto hAImage = createImage2D(hContext, hDevice, hCommandList, fmt, TOTAL_K, TOTAL_I, a);
    auto hBImage = createImage2D(hContext, hDevice, hCommandList, fmt, TOTAL_J, TOTAL_K, b);
    auto hCImage = createImage2D(hContext, hDevice, hCommandList, fmt, TOTAL_J, TOTAL_I);

    int _A_extent_0 = TOTAL_K;
    auto hKernel = createKernel(hContext, hDevice, "gemm_genx.bin", "kernel_X");
    setKernelArgs(hKernel, &_A_extent_0, &hAImage, &hBImage, &hCImage);
    L0_SAFE_CALL(zeKernelSetGroupSize(hKernel, JJ, II, 1));

    double min_thost = SIZE_MAX *1.0f;
    double thost = 0;
    // Creates a CmTask object.
    for (size_t i = 0; i < ITER; i++) {
        ze_event_handle_t hEvent = createEvent(hContext, hDevice);
        ze_group_count_t launchArgs = {J, I, 1};
        double host_start = getTimeStamp();
        appendLaunchKernel(hCommandList, hKernel, &launchArgs, hEvent);
        zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
        double host_end = getTimeStamp();

        thost += (host_end - host_start);
        if (host_end - host_start < min_thost) {
            min_thost = host_end - host_start;
        }
        if (ITER == 1) {
            float *c = (float*)malloc(sizeof(float) * SIZE_C);
            copyToMemory(hCommandList, c, hCImage, hEvent);
            zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
            check_correctness(a, b, c);
            free(c);
        }
    }
    thost = thost / ITER;
    double ops = (long)TOTAL_I*(long)TOTAL_J*(long)TOTAL_K*2.0 / (1.0f*1000*1000*1000);

    destroy(hCommandList);
    destroy(hContext);

    if (ITER == 1) {
        printf("Pass!\n");
    } else {
        cout << "Size of matrix A: " << TOTAL_I << " * " << TOTAL_K << "\n";
        cout << "Size of matrix B: " << TOTAL_K << " * " << TOTAL_J << "\n";
        printf("Average GFlops: %lf\n", ops / thost);
        printf("Max GFlops: %lf\n", ops / min_thost);
    }

    return 0;
}
