import cv2, numpy as np, sys, json
im=cv2.imread(sys.argv[1]); g=cv2.cvtColor(im,cv2.COLOR_BGR2GRAY)
pts=json.loads(sys.argv[2])
for x,y,r in pts:
    best=None
    for rr in range(int(r*0.8),int(r*1.25)):
        for dx in range(-6,7):
            for dy in range(-6,7):
                pass
    # Hough in crop
    m=int(r*1.6); x0,y0=max(0,x-m),max(0,y-m)
    crop=cv2.medianBlur(g[y0:y+m,x0:x+m],5)
    c=cv2.HoughCircles(crop,cv2.HOUGH_GRADIENT,dp=1,minDist=5,param1=100,param2=20,minRadius=int(r*0.8),maxRadius=int(r*1.2))
    if c is None: print('none',x,y); continue
    cx,cy,cr=c[0][0]; cx+=x0; cy+=y0
    # radial profile
    prof=[]
    for R in range(int(cr*0.5),int(cr*2.2),3):
        a=np.linspace(0,2*np.pi,180,endpoint=False)
        xs=(cx+R*np.cos(a)).astype(int); ys=(cy+R*np.sin(a)).astype(int)
        ok=(xs>=0)&(xs<im.shape[1])&(ys>=0)&(ys<im.shape[0])
        px=im[ys[ok],xs[ok]].astype(float)
        prof.append(f"{R}:{px[:,0].mean():.0f}/{px[:,1].mean():.0f}/{px[:,2].mean():.0f}")
    print(f"({cx:.1f},{cy:.1f}) r={cr:.1f}")
    print('  ',' '.join(prof))
