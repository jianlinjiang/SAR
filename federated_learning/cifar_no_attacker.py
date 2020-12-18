from tensorflow.keras import layers
from tensorflow.keras.layers import Dense, Conv2D, BatchNormalization, Activation
from tensorflow.keras.layers import AveragePooling2D, Input, Flatten
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.callbacks import ModelCheckpoint, LearningRateScheduler
from tensorflow.keras.callbacks import ReduceLROnPlateau
from tensorflow.keras.preprocessing.image import ImageDataGenerator
from tensorflow.keras.regularizers import l2
from tensorflow.keras import backend as K
from tensorflow.keras.models import Model
import numpy as np
import ray
import gc
import sys,getopt
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "2,3"
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

ray.init(num_gpus=2)

datagen = ImageDataGenerator(
        # set input mean to 0 over the dataset
        featurewise_center=False,
        # set each sample mean to 0
        samplewise_center=False,
        # divide inputs by std of dataset
        featurewise_std_normalization=False,
        # divide each input by its std
        samplewise_std_normalization=False,
        # apply ZCA whitening
        zca_whitening=False,
        # epsilon for ZCA whitening
        zca_epsilon=1e-06,
        # randomly rotate images in the range (deg 0 to 180)
        rotation_range=0,
        # randomly shift images horizontally
        width_shift_range=0.1,
        # randomly shift images vertically
        height_shift_range=0.1,
        # set range for random shear
        shear_range=0.,
        # set range for random zoom
        zoom_range=0.,
        # set range for random channel shifts
        channel_shift_range=0.,
        # set mode for filling points outside the input boundaries
        fill_mode='nearest',
        # value used for fill_mode = "constant"
        cval=0.,
        # randomly flip images
        horizontal_flip=True,
        # randomly flip images
        vertical_flip=False,
        # set rescaling factor (applied before any other transformation)
        rescale=None,
        # set function that will be applied on each input
        preprocessing_function=None,
        # image data format, either "channels_first" or "channels_last"
        data_format=None,
        # fraction of images reserved for validation (strictly between 0 and 1)
        validation_split=0.0)

def resnet_layer(inputs,
                 num_filters=16,
                 kernel_size=3,
                 strides=1,
                 activation='relu',
                 batch_normalization=True,
                 conv_first=True):
    conv = Conv2D(num_filters,
                  kernel_size=kernel_size,
                  strides=strides,
                  padding='same',
                  kernel_initializer='he_normal',
                  kernel_regularizer=l2(1e-4))
    x = inputs
    if conv_first:
        x = conv(x)
        if batch_normalization:
            x = BatchNormalization()(x)
        if activation is not None:
            x = Activation(activation)(x)
    else:
        if batch_normalization:
            x = BatchNormalization()(x)
        if activation is not None:
            x = Activation(activation)(x)
        x = conv(x)
    return x


def resnet_v2(input_shape, depth, num_classes=10):
    
    if (depth - 2) % 9 != 0:
        raise ValueError('depth should be 9n+2 (eg 56 or 110 in [b])')
    # Start model definition.
    num_filters_in = 16
    num_res_blocks = int((depth - 2) / 9)

    inputs = Input(shape=input_shape)
    # v2 performs Conv2D with BN-ReLU on input before splitting into 2 paths
    x = resnet_layer(inputs=inputs,
                     num_filters=num_filters_in,
                     conv_first=True)

    # Instantiate the stack of residual units
    for stage in range(3):
        for res_block in range(num_res_blocks):
            activation = 'relu'
            batch_normalization = True
            strides = 1
            if stage == 0:
                num_filters_out = num_filters_in * 4
                if res_block == 0:  # first layer and first stage
                    activation = None
                    batch_normalization = False
            else:
                num_filters_out = num_filters_in * 2
                if res_block == 0:  # first layer but not first stage
                    strides = 2    # downsample

            # bottleneck residual unit
            y = resnet_layer(inputs=x,
                             num_filters=num_filters_in,
                             kernel_size=1,
                             strides=strides,
                             activation=activation,
                             batch_normalization=batch_normalization,
                             conv_first=False)
            y = resnet_layer(inputs=y,
                             num_filters=num_filters_in,
                             conv_first=False)
            y = resnet_layer(inputs=y,
                             num_filters=num_filters_out,
                             kernel_size=1,
                             conv_first=False)
            if res_block == 0:
                # linear projection residual shortcut connection to match
                # changed dims
                x = resnet_layer(inputs=x,
                                 num_filters=num_filters_out,
                                 kernel_size=1,
                                 strides=strides,
                                 activation=None,
                                 batch_normalization=False)
            x = layers.add([x, y])

        num_filters_in = num_filters_out

    # Add classifier on top.
    # v2 has BN-ReLU before Pooling
    x = BatchNormalization()(x)
    x = Activation('relu')(x)
    x = AveragePooling2D(pool_size=8)(x)
    y = Flatten()(x)
    outputs = Dense(num_classes,
                    activation='softmax',
                    kernel_initializer='he_normal')(y)

    # Instantiate model.
    model = Model(inputs=inputs, outputs=outputs)
    return model

def create_keras_model(input_shape,depth):
    import tensorflow as tf
    gpus = tf.config.experimental.list_physical_devices("GPU")
    gpu_id = 0
    tf.config.experimental.set_visible_devices(gpus[gpu_id], "GPU")
    tf.config.experimental.set_memory_growth(gpus[gpu_id], True)
    
    model = resnet_v2(input_shape=input_shape,depth=depth)
    model.compile(loss='categorical_crossentropy',
                  optimizer=Adam(learning_rate=0.001),
                  metrics=['accuracy'])
    return model

@ray.remote (num_gpus=0.45)
class Network(object):
    def __init__(self,input_shape,depth,dataset=[],labels=[]):
        self.model = create_keras_model(input_shape,depth)
        self.dataset = dataset
        self.labels = labels
        self.clients = len(dataset)## clients num a worker process
        self.weights_list = []
        
    def train(self,r,batch_size,current_weight):
        if r == 300:
            self.model.compile(loss='categorical_crossentropy',
                  optimizer=Adam(learning_rate=0.001*0.1),
                  metrics=['accuracy'])
        for i in range(self.clients):
            datagen.fit(self.dataset[i])
            ## time cost  
            self.model.set_weights(current_weight)
            history = self.model.fit_generator(datagen.flow(self.dataset[i],self.labels[i],batch_size),
                epochs=1,verbose=0)
            weights = self.model.get_weights()
            self.weights_list.append(weights)

        return history.history

    def get_weight_list(self):
        return self.weights_list

    def clear_weight_list(self):
        self.weights_list.clear()

    def get_weights(self):
        return self.model.get_weights()

    def set_weights(self,weights):
        self.model.set_weights(weights)

    def evaluate(self,test_data,test_label,current_weight):
        self.model.set_weights(current_weight)
        test_error, test_acc = self.model.evaluate(test_data,test_label,verbose=0)
        return test_error, test_acc

def process_data_to_ray(client_iamges, client_labels, workers):
  if (len(client_iamges) != len(client_labels)):
    print("data doesn't match")
    return 
  each_worker = (int)(len(client_iamges)/workers)
  ray_images = []
  ray_labels = []
  for i in range(workers):
    tmp_images = client_iamges[i*each_worker:(i+1)*each_worker]
    tmp_labels = client_labels[i*each_worker:(i+1)*each_worker]
    ray_images.append(tmp_images)
    ray_labels.append(tmp_labels)
  return ray_images, ray_labels

workers = 4

client_images = np.load("clients_images_cifar.npy", allow_pickle=True)
client_labels = np.load("clients_labels_cifar.npy", allow_pickle=True)

test_images = np.load("test_images_cifar.npy")
test_labels = np.load("test_labels_cifar.npy")

ray_images, ray_labels = process_data_to_ray(client_images, client_labels, workers)

input_shape = client_images[0][0].shape
depth = 56
batch_size = 128
n = 100
tmp_weight = np.load("model_cifar_init_weight.npy",allow_pickle=True)
flatten_weights = []
for layer in tmp_weight:
  flatten_weights.extend(layer.flatten())
total_parameters = len(flatten_weights)
round = 500

assert (len(ray_images) == workers)
assert (len(ray_labels) == workers)

train_actors = [Network.remote(input_shape,depth,ray_images[i],ray_labels[i]) for i in range(workers)]

def federate_learning_without_attacker(gf, round, aggregation_method, m, r, k, weight):
  model_accuracy_array = []
  model_error_array = []
  tmp_weight = weight
  acc_filename = "result_data/cifar/no_attacker/acc_"+aggregation_method
  err_filename = "result_data/cifar/no_attacker/err_"+aggregation_method
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
    ray.get([actor.train.remote(r,batch_size,tmp_weight) for actor in train_actors])
    weights_list = ray.get([actor.get_weight_list.remote() for actor in train_actors])
    ray.get([actor.clear_weight_list.remote() for actor in train_actors])
    weights = weights_list[0]
    for i in range(1,workers):
      weights.extend(weights_list[i])
    if (aggregation_method == "average"):
      tmp_weight = aggregation_average(weights)
    elif (aggregation_method == "median"):
      tmp_weight = aggregation_median(weights)
    elif (aggregation_method == "trimed_mean"):
      tmp_weight = aggregation_trimed_mean(weights, gf)
    elif (aggregation_method == "krum"):
      tmp_weight = aggregation_krum(weights, n, gf, m)
    elif (aggregation_method == "sar"):
      tmp_weight = aggregation_sar(weights, n, gf, r, k)
    del weights_list
    del weights
    test_error,test_acc = ray.get(train_actors[0].evaluate.remote(test_images,test_labels,tmp_weight))
    print(r, test_error,test_acc)
    model_error_array.append(test_error)
    model_accuracy_array.append(test_acc)
    gc.collect()
  np.save(acc_filename, model_accuracy_array)
  np.save(err_filename, model_error_array)

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
  if f == '0':
    #这是没有攻击的情况
    gf = int(f)
    federate_learning_without_attacker(gf,round, method, int(m), int(float(r)* total_parameters), int(k), tmp_weight)

if __name__ == "__main__":
  main(sys.argv[1:])