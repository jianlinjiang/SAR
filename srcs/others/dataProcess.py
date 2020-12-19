import numpy as np
def save_bin(data, bin_file):
  data = data.astype(np.float32)
  data.tofile(bin_file)

mnist = np.load("model_mnist_init_weight.npy", allow_pickle=True)
for i in range(len(mnist)):
  layer = mnist[i]
  filename = "mnist_layers/"+str(i)
  layer = layer.flatten()
  save_bin(layer, filename)
cifar = np.load("model_cifar_init_weight.npy", allow_pickle=True)
for i in range(len(cifar)):
  layer = cifar[i]
  filename = "cifar_layers/"+str(i)
  layer = layer.flatten()
  save_bin(layer, filename)
