#!/bin/bash
#byzantine collude情况
python mnist_byzantine_collude.py -f 20 -c yes -a average -m 1 -r 1 -k 1
# python mnist_byzantine_collude.py -f 20 -c yes -a median -m 1 -k 1 -r 1
# python mnist_byzantine_collude.py -f 20 -c yes -a trimed_mean -m 1 -k 1 -r 1
# python mnist_byzantine_collude.py -f 20 -c yes -a krum -m 1 -k 1 -r 1
# python mnist_byzantine_collude.py -f 20 -c yes -a sar -m 1 -k 80 -r 0.1 #n-f 80