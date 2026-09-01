#include <iostream>

#include "twolayer.h"
#include "parameters.h"


void twolayer(
    hls::stream<input_t> &in,
    hls::stream<result_t> &layer4_out
) {

    // hls-fpga-machine-learning insert IO
    #pragma HLS INTERFACE axis port=in,layer4_out 
    #pragma HLS DATAFLOW

    // hls-fpga-machine-learning insert load weights
#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {
        nnet::load_weights_from_txt<model_default_t, 9>(w2, "w2.txt");
        nnet::load_weights_from_txt<model_default_t, 1>(b2, "b2.txt");
        nnet::load_weights_from_txt<model_default_t, 9>(w4, "w4.txt");
        nnet::load_weights_from_txt<model_default_t, 1>(b4, "b4.txt");
        loaded_weights = true;    }
#endif
    // ****************************************
    // NETWORK INSTANTIATION
    // ****************************************

    // hls-fpga-machine-learning insert layers

    hls::stream<layer2_t> layer2_out("layer2_out");
    #pragma HLS STREAM variable=layer2_out depth=100

    hls::stream<layer3_t> layer3_out("layer3_out");
    #pragma HLS STREAM variable=layer3_out depth=25

    nnet::conv_2d_cl<input_t, layer2_t, config2>(in, layer2_out, w2, b2); // conv1

    nnet::pooling2d_cl<layer2_t, layer3_t, config3>(layer2_out, layer3_out); // pool1

    nnet::conv_2d_cl<layer3_t, result_t, config4>(layer3_out, layer4_out, w4, b4); // conv2

}

