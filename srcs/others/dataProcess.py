import numpy as np
import os
def save_bin(data, bin_file):
  data = data.astype(np.float32)
  data.tofile(bin_file)
client_num = 1000
mnist = np.load("model_mnist_init_weight.npy", allow_pickle=True)

shape = (1,193750)
for n in range(client_num):
  path = "test_layers/"+str(n)+"/0"
  tmp = np.random.normal(0, 200, shape)
  tmp = tmp.flatten()
  save_bin(tmp, path)
# for n in range(client_num):
#   path = "mnist_layers/"+str(n)+"/"
#   for i in range(len(mnist)):
#     filename = path + str(i)
#     layer = mnist[i]
#     shape = layer.shape
#     tmp = np.random.normal(0, 200, shape)
#     tmp = tmp.flatten()
#     save_bin(tmp, filename)

# cifar = np.load("model_cifar_init_weight.npy", allow_pickle=True)
# for n in range(client_num):
#   path = "cifar_layers/"+str(n)+"/"
#   for i in range(len(cifar)):
#     filename = path+str(i)
#     layer = cifar[i]
#     shape = layer.shape
#     tmp = np.random.normal(0,200,shape)
#     tmp = tmp.flatten()
#     save_bin(tmp, filename)
