#!/bin/bash
# python cifar_backdoor.py -f 20 -c yes -a average -m 1 -r 1 -k 1
# python cifar_backdoor.py -f 20 -c yes -a sar -m 1 -r 0.1 -k 80
# python cifar_backdoor.py -f 20 -c yes -a median -m 1 -r 0.1 -k 80
python cifar_backdoor.py -f 20 -c yes -a sar2 -m 1 -r 0.1 -k 20 