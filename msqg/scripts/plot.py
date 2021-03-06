import numpy as np
import matplotlib.pyplot as plt
import sys,glob
import array

plt.ion()

dir0 = "../outdir_"

if len(sys.argv) > 1:
  dir0 = dir0 + str(format(sys.argv[1])).zfill(4) + '/'

exec(open(dir0 + "params.in").read())

filep = 'po*'


lref = 50e3
uref = 0.1

allfilesp = sorted(glob.glob(dir0 + filep));
nb_files  = len(allfilesp);

b = np.fromfile(allfilesp[0],'f4')
N = int(b[0])
N1 = N + 1
nl = int(len(b)/N1**2)


y = np.arange(N)
x = np.arange(N)

xc,yc = np.meshgrid(x,y)

psipg = np.fromfile(dir0 + 'psipg_' + str(nl) + 'l_N'+ str(N) + '.bas' ,'f4').reshape(nl,N1,N1).transpose(0,2,1)
psipg = psipg[:,1:,1:]

frpg = np.fromfile(dir0 + 'frpg_' + str(nl) + 'l_N'+ str(N) + '.bas','f4').reshape(nl,N1,N1).transpose(0,2,1)
frpg = frpg[:,1:,1:]


# read constants
#iBu = np.fromfile(dir0 + 'iBu.bas','f4').reshape(nl,N1,N1).transpose(0,2,1)
#iBu = iBu[:,1:,1:]
#Rd = np.sqrt(-1/iBu)*lref[1,:,:]

rdpg = np.fromfile(dir0 + 'rdpg_' + str(nl) + 'l_N'+ str(N) + '.bas','f4').reshape(N1,N1).transpose(1,0)
Rd = rdpg[1:,1:]


plt.figure()
CS = plt.contour(xc,yc,Rd*lref*1e-3,[1,2,5,10,20,30,40,50,60,70,80,90],colors='b')
plt.clabel(CS, inline=1, fontsize=10)

# choose vertical level (0= top, nl-1=bottom)
l = 0

ifi0 = nb_files - 1
#ifi0 = 1
#ifi0 = 24

for ifi in range(ifi0,nb_files):

  p  = np.fromfile(allfilesp[ifi],'f4').reshape(nl,N1,N1).transpose(0,2,1)
  p = p[:,1:,1:]

  plt.figure()
  plt.clf()
  vcont = np.linspace(-500,500,50)

  plt.contour(xc,yc,psipg[l,:,:] + p[l,:,:],colors='k',linewidths=1)
#  plt.contourf(xc,yc,psipg[l,:,:], vcont,cmap=plt.cm.seismic)
  plt.colorbar()
  
#  plt.savefig("psi{0:04d}.png".format(ifi),bbox_inches='tight')
  plt.draw()
  
#  nextit = input("press enter")
