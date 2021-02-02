import numpy as np
import random
import tensorflow as tf
from tensorflow.keras.optimizers import Adam
import tensorflow.keras as keras
import sys, getopt
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "2"
# 模型参数聚合函数
# 传入参数是模型数据的列表
def aggregation_average(model_weights):
    average_model = np.mean(model_weights,axis = 0)
    return average_model

##median 
def aggregation_median(model_weights):
    #总共有多少个模型
    number = len(model_weights)
    #一个模型有多少层
    layerNum = len(model_weights[0])
    result = []
    for i in range(layerNum):
        tmp_weight = []
        #把每个模型的这一层的参数加进来求平均
        for j in range(number):
            tmp_weight.append(model_weights[j][i])
        result.append(np.median(tmp_weight, axis=0))
    return result

#trimed mean 
#对模型参数排序后
def aggregation_trimed_mean(model_weights, beta):
    number = len(model_weights)
    layerNum = len(model_weights[0])
    if (2*beta>= number):
        print ("beta must be larger than number/2")
        return 
    result = []
    for i in range(layerNum):
        weight = []
        shape = model_weights[0][i].shape
        for j in range(number):
            weight.append(model_weights[j][i].flatten())
        # 每一层展开后加入到weight中，这时weight是一个二维数组，对每一列进行排序然后选择
        weight = np.array(weight)
        num = len(weight[0])
        tmp = np.zeros(num, dtype=np.float32)
        for j in range(num):
            column = weight[:,j]
            column.sort()
            tmp[j] = np.mean(column[beta:number-beta], axis=0)
        result.append(tmp.reshape(shape))
    return result

# m 代表multi krum中的参数
def aggregation_krum(model_weights, n, f, m):
    if (n - 2 * f -2 <= 0):
        print("krum condition doesn't satisfy")
        return 
    number = len(model_weights)
    layerNum = len(model_weights[0])
    distance = np.zeros((number, number))
    for i in range(number):
        for j in range(i+1, number):
            d = 0
            for k in range(layerNum):
                d += np.linalg.norm(np.array(model_weights[i][k]) - np.array(model_weights[j][k]))
                distance[i][j] = d
    distance += distance.T
    #矩阵i，j 代表第i个客户端到第j个客户端的的距离
    score = np.zeros((number))
    #代表得分
    for i in range(number):
        d = distance[i] #第i行，代表该模型到其他模型之间到距离
        np.sort(d) #排序
        for j in range(1,n-f-1): #因为有一个0，到自身到距离，排除掉，选出距离最小到n-f-2个客户端到距离
            score[i] += d[j]
    sorted_weights = []
    indexs = np.argsort(score)
    for i in range(m):
        sorted_weights.append(model_weights[indexs[i]])
    return aggregation_average(sorted_weights)

# m 代表从模型中随机挑出多少参数来做距离计算
# K 代表选多少个模型参数来做中位数计算
def aggregation_sar(model_weights, n, f, r, k):
    if (n - 2 * f - 2 <= 0):
        print("sar condition doesn't satisfy")
        return
    if k > n:
        print("sar condition doesn't satisfy")
        return
    number = len(model_weights)
    layerNum = len(model_weights[0])
    # 把模型参数展开
    flattened_weights = []
    for i in range(number):
        flattened = []
        for layer in model_weights[i]:
            flattened.extend(layer.flatten())
        flattened_weights.append(flattened)
    total_parameters = len(flattened)
    choices = np.random.choice(total_parameters, r)
    #choices 代表从这些参数中选出来的下标
    #samped 代表随机挑选出来的参数
    sampled_weights = []
    for i in range(number):
        sampled = []
        for j in range(r):
            sampled.append(flattened_weights[i][choices[j]])
        sampled_weights.append(np.array(sampled))
    #在这些sampled参数上计算
    distance = np.zeros((number,number))
    for i in range(number):
        for j in range(i+1, number):
            distance[i][j] = np.linalg.norm(np.array(sampled_weights[i]) - np.array(sampled_weights[j]))
            #在挑出的样本上做距离的计算
    distance += distance.T   
    #评分
    score = np.zeros((number))
    for i in range(number):
        d = distance[i]
        np.sort(d)
        for j in range(1,n-f-1):
            score[i] += d[j]

    #在所有的模型参数上
    aggregation_weights = []
    indexs = np.argsort(score)
    for i in range(k):
        aggregation_weights.append(model_weights[indexs[i]])
    return aggregation_median(aggregation_weights)

def aggregation_sar2(model_weights, n, f, r, k):
  if (n - 2 * f -2 <= 0):
    print("krum condition doesn't satisfy")
    return 
  number = len(model_weights)
  layerNum = len(model_weights[0])
  distance = np.zeros((number, number))
  for i in range(number):
    for j in range(i+1, number):
      d = 0
      for l in range(layerNum):
        d += np.linalg.norm(np.array(model_weights[i][l]) - np.array(model_weights[j][l]))
      distance[i][j] = d
  distance += distance.T
  score = np.zeros((number))
  for i in range(number):
    d = distance[i] #第i行，代表该模型到其他模型之间到距离
    np.sort(d) #排序
    for j in range(1,n-f-1): #因为有一个0，到自身到距离，排除掉，选出距离最小到n-f-2个客户端到距离
      score[i] += d[j]
  sorted_weights = []
  indexs = np.argsort(score)
  for i in range(k):
    sorted_weights.append(model_weights[indexs[i]])
  return aggregation_median(sorted_weights)
# 定义全局模型
model_cnn = tf.keras.Sequential([
  tf.keras.layers.Conv2D(filters=32,activation=tf.nn.relu,kernel_size=(5,5), input_shape=(28,28,1),strides = (1,1),padding = 'same',name='Con1'),
  tf.keras.layers.MaxPooling2D(pool_size=(2,2),name='Maxpool1'),
  tf.keras.layers.Conv2D(filters=64,activation=tf.nn.relu,kernel_size=(5,5),strides = (1,1),padding = 'same',name='Con2'),
  tf.keras.layers.MaxPooling2D(pool_size=(2,2),name='Maxpool2'), 
  tf.keras.layers.Dropout(0.25,name='Dropout1'),
  tf.keras.layers.Flatten(name='Flatten1'),
  tf.keras.layers.Dense(512, activation=tf.nn.relu,name='Dense1'),
  tf.keras.layers.Dropout(0.25,name='Dropout2'),
  tf.keras.layers.Dense(10, activation=tf.nn.softmax,name='Dense2')    
])
model_cnn.compile(loss='categorical_crossentropy',
                  optimizer=Adam(learning_rate=0.01),
                  metrics=['accuracy'])

model_init_weights = np.load("model_mnist_init_weight.npy",allow_pickle=True)
model_cnn.set_weights(model_init_weights)

#所有客户端数量
n = 100

#读取文件
client_images_non = np.load("clients_images_mnist_noniid.npy",allow_pickle=True)
client_labels = np.load("clients_labels_mnist_noniid.npy",allow_pickle=True)
client_images = []
for i in range(n):
  client_images.append(keras.backend.expand_dims(client_images_non[i], axis=-1))
test_images = np.load("test_images_mnist.npy", allow_pickle=True)
test_labels = np.load("test_images_label.npy", allow_pickle=True)

if (len(client_images) != n or len(client_labels) != n):
  raise RuntimeError("data donesn't match")

batch_size = 32
round = 200
gf = 0
flatten_weights = []
for layer in model_init_weights:
  flatten_weights.extend(layer.flatten())
total_parameters = len(flatten_weights)

def training(model, data, label, weights, bz):
  model.fit(data, label, epochs=1, verbose=0, batch_size=bz)
  weights.append(model.get_weights())

def federate_learning_without_attacker(gf, round, aggregation_method, m, r, k, model):
  model_accuracy_array = []
  model_error_array = []
  tmp_weight = model.get_weights()
  acc_filename = "result_data/no_attacker/noniidacc_"+aggregation_method
  err_filename = "result_data/no_attacker/noniiderr_"+aggregation_method
  if (aggregation_method == "trimed_mean"):
    acc_filename += "_beta:"+str(gf)
    err_filename += "_beta:"+str(gf)
  if (aggregation_method == "krum"):
    acc_filename += "_m:"+str(m)
    err_filename += "_m:"+str(m)
  if (aggregation_method == "sar"):
    acc_filename += "_r:"+str(r)+"_k:"+str(k)
    err_filename += "_r:"+str(r)+"_k:"+str(k)
  for r in range(round):
    model_weights = []
    for i in range(n):
      model.set_weights(tmp_weight)
      training(model, client_images[i], client_labels[i], model_weights, batch_size)
    if (aggregation_method == "average"):
      tmp_weight = aggregation_average(model_weights)
    elif (aggregation_method == "median"):
      tmp_weight = aggregation_median(model_weights)
    elif (aggregation_method == "trimed_mean"):
      tmp_weight = aggregation_trimed_mean(model_weights, gf)
    elif (aggregation_method == "krum"):
      tmp_weight = aggregation_krum(model_weights, n, gf, m)
    elif (aggregation_method == "sar"):
      tmp_weight = aggregation_sar2(model_weights, n, gf, r, k)
    model.set_weights(tmp_weight)
    test_error,test_acc = model.evaluate(test_images,test_labels)
    model_accuracy_array.append(test_acc)
    model_error_array.append(test_error)
  np.save(acc_filename, model_accuracy_array)
  np.save(err_filename, model_error_array)
#参数 
#第一个参数 f 代表 恶意的人数
#第二个参数 collude 代表是否合谋，合谋即攻击者模型参数为很大的值，否则为各自随机产生的噪声
#第三个参数 method 聚合方法 average median trimed_mean krum sar
#其中krum 有额外参数 m
#sar 有额外参数m抽样比例 K代表聚合个数

def main(argv):
  
  collude = "yes"
  method = "average"
  m = 1
  k = 1
  r = 0.1
  try:
    opts, args = getopt.getopt(argv, "f:c:a:m:r:k:")
  except getopt.GetoptError:
    print("python mnist -f <malicious_count> -c <yes or no> -a <aggregation method>")
  for opt, arg in opts:
    if opt in ("-f"):
      f = arg
    elif opt in ("-c"):
      collude = arg
    elif opt in ("-a"):
      method = arg
    elif opt in ("-m"):
      m = arg
    elif opt in ("-r"): #随机抽样的样本数 小数
      r = arg
    elif opt in ("-k"): #最后平均数的数量
      k = arg
  gf = int(f)
  federate_learning_without_attacker(gf, round, method, int(m), int(float(r)* total_parameters), int(k), model_cnn)
  #如果合谋，则最后20个客户端的模型参数全部改为非常大的值在原模型

if __name__ == "__main__":
  main(sys.argv[1:])
