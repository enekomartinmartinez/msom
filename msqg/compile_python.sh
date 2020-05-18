
# just for legacy.
# do not use: use makefile instead

qcc -O2 -g -Wall -pipe -D_FORTIFY_SOURCE=2 -python -c -fpic -I/mnt/meom/workdir/martiene/anaconda3/envs/env/include/python3.7m qg.c
#qcc -O2 -g -Wall -pipe -D_FORTIFY_SOURCE=2 -c -fpic -I/usr/include/python3.8 qg.c
swig -I/mnt/meom/workdir/martiene/basilisk/src -python -py3 qg.i

cc -O2 -g -Wall -pipe -D_FORTIFY_SOURCE=2 -c -fpic -I/mnt/meom/workdir/martiene/anaconda3/envs/env/include/python3.7m -I/mnt/meom/workdir/martiene/anaconda3/envs/env/include/ qg_wrap.c

cc -shared qg.o qg_wrap.o -o _qg.so
