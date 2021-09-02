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
// To debug the correctness of the UREs (on the host):
//    g++ LU-basic.cpp -O0 -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DDEBUG_URES -DSIZE=1(or 2, 3,..., 6)
//    ./a.out
//
// To compile and run on the FPGA emulator:
//    g++ LU-basic.cpp -O0 -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DSIZE=1(or 2, 3,..., 6)
//    rm ~/tmp/a.* ~/tmp/a -rf
//    env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 AOC_OPTION="-march=emulator -O0 -g " ./a.out

#include "util.h"

#define TYPE        float
#define HALIDE_TYPE Float(32)


int main(void) {
    ImageParam A(type_of<TYPE>(), 2);

    // Macros: for convenient use.
    #define X                      i,     j,     k
    #define X_no_k                 i,     j
    #define X_k_minus_1            i,     j,     k - 1
    #define X_j_minus_1            i,     j - 1, k
    #define X_i_minus_1            i - 1, j,     k
    #define FUNC_DECL              HALIDE_TYPE, {X}, PLACE1

    Var  X;
    Func PrevV(FUNC_DECL), V(FUNC_DECL), L(FUNC_DECL), U(FUNC_DECL), Z(FUNC_DECL), // A recurrent Func needs declare return type, args, and place.
         O(PLACE1);         // A non-recurrent Func needs declare only its place.

    PrevV(X)  = select(i >= k, select(k == 0, A(i, j), V(X_k_minus_1)), 0);
    U(X)      = select(i >= k, select(j == k, PrevV(X), U(X_j_minus_1)), 0);
    L(X)      = select(j == k || i < k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               select(i == k, PrevV(X) / U(X_j_minus_1), L(X_i_minus_1)));
    V(X)      = select(j == k || i < k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               PrevV(X) - L(X) * U(X_j_minus_1));
    Z(X)      = select(i >= k, select(j == k, U(X), select(i == k, L(X), 0)), select(k > 0, Z(X_k_minus_1), 0));

    O(X_no_k) = select(j == k, Z(X));

    PrevV.merge_ures(U, L, V, Z, O) // Put all the UREs into the same loop nest
         .reorder(j, k, i)
         .set_bounds(k, 0, SIZE, j, k, SIZE - k, i, 0, SIZE)
         .space_time_transform(j, k);

    Func serializer(PLACE0), feeder(PLACE1), loader(PLACE1);
    PrevV.isolate_producer_chain(A, serializer, loader, feeder);
    serializer.set_bounds(k, 0, 1, j, 0, SIZE);
    loader.set_bounds(k, 0, 1, j, 0, SIZE);
    feeder.set_bounds(k, 0, 1, j, 0, SIZE);
    feeder.scatter(loader, j);
    feeder.min_depth(512);
    //feeder.buffer(loader, j, BufferStrategy::Double);

    Func deserializer(PLACE0), collector(PLACE1), unloader(PLACE1);
    O.isolate_consumer_chain(collector);
    collector.space_time_transform(j)
	     .set_bounds(i, 0, SIZE)
             .set_bounds(j, 0, SIZE);
    collector.isolate_consumer_chain(unloader);
    collector.gather(O, j);
    unloader.isolate_consumer_chain(deserializer);
    
    // Generate input and run.
    // A.dim(0).set_bounds(0, SIZE).set_stride(1);
    // A.dim(1).set_bounds(0, SIZE).set_stride(SIZE);
    //unloader.output_buffer().dim(0).set_bounds(0, SIZE).set_stride(1);
    //unloader.output_buffer().dim(1).set_bounds(0, SIZE).set_stride(SIZE);

    // To manually verify, you can use the tool at https://www.iotools.net/math/lu-factorization-calculator
#if SIZE == 1
    TYPE data[] = {2};  // Expect output: 2
    Buffer<TYPE> input(data, 1, 1);
#elif SIZE == 2
    TYPE data[] = { 1, 3, 4, 5}; // Expect output: 1 3; 4 -7
    Buffer<TYPE> input(data, 2, 2);
#elif SIZE == 3
    TYPE data[] = { 1, 3, 9, 4, 6, 10, 2, 5, 3}; // Expect output: 1 3 9; 4 -6 -26; 2 0.17 -10.67
    Buffer<TYPE> input(data, 3, 3);
#elif SIZE == 4
    TYPE data[] = { 1, 4, 5, 2, 2, 3, 6, 7, 3, 5, 8, 9, 5, 2, 1, 12}; // Expect output: 1 4 5 2; 2 -5 -4 3; 3 1.4 -1.4 -1.2; 5 3.6 6.86 -0.57
    Buffer<TYPE> input(data, 4, 4);
#elif SIZE == 5
    TYPE data[] = { 2, 4, 5, 2, 5, 1, 4, 6, 7, 6, 3, 5, 5, 9, 7, 5, 2, 1, 6, 8, 5, 6, 7, 8, 7}; // Expect output: 2 4 5 2 5; 0.5 2 3.5 6 3.5; 1.5 -0.5 -0.75 9 1.25; 2.5 -4 -3.33 55 13.67; 2.5 -2 -2 0.6 -4.2
    Buffer<TYPE> input(data, 5, 5);
#elif SIZE == 6
    TYPE data[] = { 3, 1, 2, 5, 4, 6, 1, 2, 3, 7, 6, 8, 3, 4, 5, 9, 7, 7, 5, 10, 1, 2, 8, 8, 5, 6, 5, 5, 10, 9, 6, 3, 7, 9, 11, 12};
    Buffer<TYPE> input(data, 6, 6);
#else
    assert(false);
#endif

    // Note: If the input is not set as dirty, it won't be copied to the device.
    input.set_host_dirty();

    A.set(input);
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
    Buffer<TYPE> results = deserializer.realize({SIZE, SIZE}, target);

    void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input);
    check(results, input);
    cout << "Success!\n";
    return 0;
}

void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input) {
    printf("*** Input:\n");
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
        printf("%5.2f ", input(i, j));
      }
      printf("\n");
    }

    // Do exactly the same compute in C style
    Buffer<TYPE> PrevV(SIZE, SIZE, SIZE), V(SIZE, SIZE, SIZE), L(SIZE, SIZE, SIZE), U(SIZE, SIZE, SIZE), O(SIZE, SIZE);
    for (int k = 0; k < SIZE; k++) {
      for (int j = k; j < SIZE; j++) {
        for (int i = k; i < SIZE; i++) {
          if (k == 0) {
              PrevV(i, j, k) = input(i, j);
          } else {
              PrevV(i, j, k) = V(i, j, k - 1);
          }

          if (j == k) {
              U(i, j, k) = PrevV(i, j, k);
          } else {
              // operation f: No change to row k, because we do not do pivoting
              U(i, j, k) = U(i, j - 1, k);

              // operation g
              if (i == k) {
                  L(i, j, k) = PrevV(i, j, k) / U(i, j - 1, k);
              } else {
                  L(i, j, k) = L(i - 1, j, k);
              }
              V(i, j, k) = PrevV(i, j, k) - L(i, j, k) * U(i, j - 1, k);
          }

           // Final results in the decomposition include
           if (j == k) {
               O(i, j) =  U(i, j, k);
           } else {
               if (i == k) {
                   O(i, j) =  L(i, j, k);
               }
           }
        }
      }
    }

    // Check if the results are the same as we get from the C style compute
    printf("*** C style result (URE style):\n");
    bool pass = true;
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
            bool correct = (abs(O(i, j) - results(i, j)) < 1e-2);
            printf("%5.2f (%5.2f%s)", O(i, j), results(i, j), correct ? "" : " !!");
            pass = pass && correct;
        }
        printf("\n");
    }

    // Check if the results can reproduce the input, i.e. L*U = A
    printf("*** L * U in C style (Input):\n");
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
        TYPE sum = 0;
        // j'th row of O times i'th column of O
        for (int k = 0; k < SIZE; k++) {
            TYPE l, u;
            l = (j > k) ? O(k, j) : (j == k) ? 1 : 0;
            u = (k > i) ? 0 : O(i, k);
            sum += l * u;
        }
        bool correct = (abs(sum - input(i, j)) < 1e-2);
        printf("%5.2f (%5.2f%s)", sum, input(i, j), correct ? "" : " !!");
        pass = pass && correct;
      }
      printf("\n");
    }

    fflush(stdout);
    assert(pass);
}
