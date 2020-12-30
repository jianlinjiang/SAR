#!/bin/bash
# python mnist_backdoor.py -f 20 -c yes -a average -m 1 -r 1 -k 1
python mnist_backdoor.py -f 20 -c yes -a median -m 1 -r 1 -k 1
python mnist_backdoor.py -f 20 -c yes -a trimed_mean -m 1 -r 1 -k 1
python mnist_backdoor.py -f 20 -c yes -a krum -m 1 -r 1 -k 1
python mnist_backdoor.py -f 20 -c yes -a sar -m 1 -r 0.1 -k 100