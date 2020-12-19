#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include "ra_service/ra_message.h"
#include "ra_service/sar_message.h"
#include <atomic>
#include <thread>
#include <vector>
#include <cstring>
#include <condition_variable>
using namespace std;
atomic<int> x(0);
int arr[100];
condition_variable cond;
mutex m;
bool start = false;
void f()
{
  unique_lock<mutex> l(m);
  cond.wait(l, []() { return start; });
  int y = x.load();
  while (!x.compare_exchange_strong(y, x + 1))
  {
  }
  arr[y]++;
  // int y = x++;
  // arr[y]++;
}
int main()
{
  memset(arr, 0, sizeof(arr));
  ifstream weights_file;
  int layer_num = 8;
  string filename = "mnist_layers/mnist:";
  float data[8192]; //32k 大小的数组
  memset(data, 0, sizeof(data));
  for (int i = 0; i < layer_num; i++)
  {
    weights_file.open(filename + to_string(i), ios::in | ios::binary);
    if (weights_file.is_open())
    {
      size_t read_bytes = weights_file.readsome((char *)data, sizeof(data));
      printf("%lu\n", read_bytes);
      //判断是否到结尾了
      // while (!weights_file.eof())
      break;
    }
    else
    {
      printf("error file is not open!\n");
      return 0;
    }
  }
  printf("\nra size:%lu\n", sizeof(sar::transmit_message));

  vector<thread> v;
  for (int i = 0; i < 100; i++)
  {
    v.push_back(move(thread(f)));
  }
  start = true;
  cond.notify_all();
  for (int i = 0; i < 100; i++)
  {
    v[i].join();
  }
  for (int i = 0; i < 100; i++)
  {
    printf("%d ", arr[i]);
  }
  return 0;
}