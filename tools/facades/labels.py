import cv2, numpy as np, sys, json, math
im=cv2.imread(sys.argv[1]); g=cv2.cvtColor(im,cv2.COLOR_BGR2GRAY)
for spec in json.loads(sys.argv[2]):
    cx,cy,r0,r1=spec
    H,W=g.shape
    yy,xx=np.mgrid[0:H,0:W]; d=np.hypot(xx-cx,yy-cy)
    m=((d>r0)&(d<r1)&(g>150)).astype(np.uint8)
    m=cv2.dilate(m,np.ones((5,5),np.uint8))
    n,lab,st,cen=cv2.connectedComponentsWithStats(m)
    out=[]
    for i in range(1,n):
        x,y,w,h,a=st[i]
        if a<15: continue
        px,py=cen[i]; ang=math.degrees(math.atan2(px-cx,-(py-cy)))
        out.append((round(ang),round(math.hypot(px-cx,py-cy)),w,h))
    print(spec, sorted(out))
