import numpy as np, hls4ml
from hls4ml.model import ModelGraph

# equivalent net: input 12x12x1 -> Conv2D 3x3 (1 filt, valid) -> MaxPool 2x2
#                 -> Conv2D 3x3 (1 filt, valid). io_stream.
cfg = {
  'OutputDir': '/tmp/claude-1000/-home-azevedo-Work-r-site-net-C---IOP/2fdfa036-55ce-4073-a773-c8eaaa5b1340/scratchpad/h4_prj',
  'ProjectName': 'twolayer',
  'Backend': 'Vivado',
  'Part': 'xc7a100tcsg324-1',
  'ClockPeriod': 10,
  'IOType': 'io_stream',
  'HLSConfig': {'Model': {'Precision': 'ap_fixed<16,8>', 'ReuseFactor': 1, 'Strategy': 'Latency'}},
}

W1 = np.zeros((3,3,1,1), dtype=np.float32); W1[...,0,0] = np.array([[26,-77,141],[-8,210,-119],[45,-163,92]])/256.0
b1 = np.zeros((1,), dtype=np.float32)
W2 = np.zeros((3,3,1,1), dtype=np.float32); W2[...,0,0] = np.array([[-33,64,-5],[128,-200,17],[-71,9,150]])/256.0
b2 = np.zeros((1,), dtype=np.float32)

layers = [
  {'class_name':'InputLayer','name':'in','input_shape':[12,12,1]},
  {'class_name':'Conv2D','name':'conv1','n_filt':1,'filt_height':3,'filt_width':3,
   'stride_height':1,'stride_width':1,'padding':'valid','data_format':'channels_last','pad_top':0,'pad_bottom':0,'pad_left':0,'pad_right':0,
   'in_height':12,'in_width':12,'n_chan':1,'out_height':10,'out_width':10,
   'weight_data':W1,'bias_data':b1,'activation':'linear'},
  {'class_name':'MaxPooling2D','name':'pool1','pool_height':2,'pool_width':2,
   'stride_height':2,'stride_width':2,'padding':'valid','data_format':'channels_last','pad_top':0,'pad_bottom':0,'pad_left':0,'pad_right':0,
   'in_height':10,'in_width':10,'n_filt':1,'out_height':5,'out_width':5},
  {'class_name':'Conv2D','name':'conv2','n_filt':1,'filt_height':3,'filt_width':3,
   'stride_height':1,'stride_width':1,'padding':'valid','data_format':'channels_last','pad_top':0,'pad_bottom':0,'pad_left':0,'pad_right':0,
   'in_height':5,'in_width':5,'n_chan':1,'out_height':3,'out_width':3,
   'weight_data':W2,'bias_data':b2,'activation':'linear'},
]
try:
    model = ModelGraph.from_layer_list(cfg, layers)
    model.write()
    print("WROTE PROJECT")
except Exception as e:
    import traceback; traceback.print_exc()
