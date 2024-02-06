import numpy

f=open("out.data","rb")
buffer=f.read()
f.close()
x=numpy.frombuffer(buffer,dtype=numpy.float64)

itn=1024
m=numpy.diag(x[1:itn+1])
for i in range(itn-1):
    m[i,i+1]=x[itn+1+i]
    m[i+1,i]=x[itn+1+i]
[e,v]=numpy.linalg.eigh(m)
c=x[0]*v[0,:]

ep=-15+numpy.arange(4501)*0.01
sp=numpy.zeros(4501)
ga=0.125
for i in range(itn):
    sp=sp+0.5*ga*c[i]*c[i]/(0.25*ga*ga+(e[i]-ep)*(e[i]-ep))

f=open("sp.data","rb")
buffer=f.read()
f.close()
spref=numpy.frombuffer(buffer,dtype=numpy.float64)

if numpy.sqrt(((sp-spref)*(sp-spref)).sum())<0.005:
    exit(0)
else:
    exit(1)
