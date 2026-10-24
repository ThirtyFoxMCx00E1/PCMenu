import sys
from PIL import Image
import os
ROOT=os.environ.get('PCM_DIR','../../files/PCMenu').rstrip('/')+'/pc'
W=int(sys.argv[3]); H=int(sys.argv[4])
tex={1:Image.open(f'{ROOT}/fonts/bank.png').convert('RGBA'),2:Image.open(f'{ROOT}/fonts/beck.png').convert('RGBA'),5:Image.open(f'{ROOT}/fonts/bank_ar.png').convert('RGBA'),6:Image.open(f'{ROOT}/fonts/beck_ar.png').convert('RGBA'),3:Image.open(f'{ROOT}/images/arrow.png').convert('RGBA'),4:Image.open(f'{ROOT}/images/back8.png').convert('RGBA')}
for n in (2,3,4,5): tex[10+n]=Image.open(f'{ROOT}/images/back{n}.png').convert('RGBA')
cv=Image.new('RGBA',(W,H),(0,0,0,255))
for ln in open(sys.argv[1]):
    p=ln.split()
    if p[0]=='Q':
        v=list(map(float,p[1:9])); r,g,b,a=map(int,p[9:13])
        x0,y0,x1,y1=v[0],v[1],v[6],v[7]
        ov=Image.new('RGBA',(max(1,int(round(x1-x0))),max(1,int(round(y1-y0)))),(r,g,b,a)); cv.alpha_composite(ov,(int(round(x0)),int(round(y0)))) if x0>=0 and y0>=0 else None
    else:
        t=int(p[1]); v=list(map(float,p[2:10])); uv=list(map(float,p[10:18])); r,g,b,a=map(int,p[18:22])
        im=tex[t]; tw,th=im.size
        x0,y0,x1,y1=v[0],v[1],v[6],v[7]
        u0,v0,u1,v1=uv[0],uv[1],uv[6],uv[7]
        flip=u1<u0
        if flip: u0,u1=u1,u0
        crop=im.crop((int(round(u0*tw)),int(round(v0*th)),max(int(round(u1*tw)),int(round(u0*tw))+1),max(int(round(v1*th)),int(round(v0*th))+1)))
        if flip: crop=crop.transpose(Image.FLIP_LEFT_RIGHT)
        w=max(1,int(round(x1-x0))); h=max(1,int(round(y1-y0)))
        crop=crop.resize((w,h),Image.BILINEAR)
        if (r,g,b)!=(255,255,255) or a!=255:
            px=crop.load()
            from PIL import ImageChops
            tint=Image.new('RGBA',crop.size,(r,g,b,255)); 
            rgb=ImageChops.multiply(crop.convert('RGB'),tint.convert('RGB'))
            al=crop.getchannel('A').point(lambda x:x*a//255)
            crop=rgb.convert('RGBA'); crop.putalpha(al)
        X,Y=int(round(x0)),int(round(y0))
        # clip to canvas
        if X<0 or Y<0:
            crop=crop.crop((max(0,-X),max(0,-Y),crop.size[0],crop.size[1])); X=max(X,0); Y=max(Y,0)
        cv.alpha_composite(crop,(X,Y))
cv.convert('RGB').save(sys.argv[2]); print('saved',sys.argv[2])
