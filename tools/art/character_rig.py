#!/usr/bin/env python3
"""Authoring tool: the player body frames, built from one set of parts (ADR-078).

NOT part of the normal build; run by hand, then import_character_art.py.
Standard library only. Each frame is a pose: the head's top-left, the
torso's column, and the joints of each limb (shoulder, elbow, hand; hip,
knee, ankle), plus where the paddle is. The parts (head, torso, shorts,
2-pixel arms, 3-pixel legs, shoes, paddle) are drawn the same way in every
frame, back limbs in the darker skin, and outlined last, so the colours
stay in their roles and a pose is edited by moving a joint.

Writes art/characters/sprites/frames/<frame>.txt for every pose below
and the head boxes in heads.json. The dive is a hand-drawn grid and is
left alone (its head box is read from it).

Usage: python tools/art/character_rig.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art" / "characters" / "sprites"

def blank(): return [['.']*32 for _ in range(32)]
def put(g,r,c,s):
    for i,ch in enumerate(s):
        if ch!=' ' and 0<=r<32 and 0<=c+i<32: g[r][c+i]=ch
def pix(pts):
    out=[]
    for (r0,c0),(r1,c1) in zip(pts,pts[1:]):
        dr=abs(r1-r0); dc=abs(c1-c0); sr=1 if r1>r0 else -1; sc=1 if c1>c0 else -1
        err=dc-dr; r,c=r0,c0
        while True:
            if not out or out[-1]!=(r,c): out.append((r,c))
            if (r,c)==(r1,c1): break
            e2=2*err
            if e2>-dr: err-=dr; c+=sc
            if e2<dc: err+=dc; r+=sr
    return out
def limb(g,pts,main='3',shade='2',off=(0,1),first='',):
    """2-pixel limb: a shade line offset by off, then the main line; the
    first len(first) pixels (sleeve or shorts leg) take those colours."""
    p=pix(pts)
    for i,(r,c) in enumerate(p):
        ch=first[i] if i<len(first) else shade
        put(g,r+off[0],c+off[1],{'6':'5','9':'8'}.get(ch,ch) if i<len(first) else shade)
    for i,(r,c) in enumerate(p):
        put(g,r,c,first[i] if i<len(first) else main)
def head(g,T,L,bow=False):
    put(g,T,L+1,'11111'); put(g,T+1,L,'1FFFFF1'); put(g,T+2,L,'1FFF331')
    if bow: put(g,T+3,L,'1F33331'); put(g,T+4,L,'1F23131')
    else: put(g,T+3,L,'1F33131'); put(g,T+4,L,'1F23331')
    put(g,T+5,L+1,'12331')
def torso(g,r,c,h=7):
    """Shirt, 8 wide from column c, shoulders on row r, h rows."""
    put(g,r,c+1,'566667')
    for i in range(1,h-1): put(g,r+i,c,'56666667' if i<3 else ' 5666667')
    put(g,r+h-1,c,' 5555555')
def collar(g,r,L):
    put(g,r,L+2,'32')
def shorts(g,r,c,w=8):
    put(g,r,c+1,'9'*(w-2)+'8'); put(g,r+1,c,'9'*(w-2)+'88')
def shoe(g,r,c,toe=1):
    put(g, r, c, '8AA' if toe > 0 else ' AA8'); put(g, r + 1, c, 'AAAA')
def paddle(g,r,c,flat=False):
    if flat: put(g,r,c+1,'CCCC'); put(g,r+1,c,'BBBBBB'); return
    put(g,r,c+1,'CCC'); put(g,r+1,c,'CECCB'); put(g,r+2,c,'CCCBB'); put(g,r+3,c,'CCBBB'); put(g,r+4,c+1,'BBB')
def handle(g,pts):
    for r,c in pix(pts): put(g,r,c,'D')
def outline(g):
    for _ in range(2):
        for r in range(1,31):
            for c in range(1,31):
                if g[r][c]=='.' and sum(g[r+a][c+b]!='.' for a,b in((1,0),(-1,0),(0,1),(0,-1)))>=3: g[r][c]='1'
    add=[]
    for r in range(32):
        for c in range(32):
            if g[r][c]!='.': continue
            for dr,dc in((1,0),(-1,0),(0,1),(0,-1)):
                rr,cc=r+dr,c+dc
                if 0<=rr<32 and 0<=cc<32 and g[rr][cc] not in '.1': add.append((r,c)); break
    for r,c in add: g[r][c]='1'
    for r in range(1,31):
        for c in range(1,31):
            if g[r][c]=='.' and sum(g[r+a][c+b]!='.' for a,b in((1,0),(-1,0),(0,1),(0,-1)))>=3: g[r][c]='1'
def ground(g):
    rows=[r for r in range(32) if any(ch!='.' for ch in g[r])]
    sh=29-max(rows)
    if sh>0: g[:]=[['.']*32 for _ in range(sh)]+g[:-sh]
    elif sh<0: g[:]=g[-sh:]+[['.']*32 for _ in range(-sh)]
    return sh
def leg(g,pts,dark=False,first='999'):
    p=pix(pts)
    m,e=('2','3') if dark else ('3','4')
    for i,(r,c) in enumerate(p):
        sh = first[i] if i<len(first) else '2'
        put(g,r,c-1,{'9':'8'}.get(sh,sh)); 
    for i,(r,c) in enumerate(p):
        put(g,r,c+1,first[i] if i<len(first) else (e if i>len(p)//2-1 else m))
    for i,(r,c) in enumerate(p):
        put(g,r,c,first[i] if i<len(first) else m)

def std(T=3,L=13,tc=None):
    """Upright body: head top T, head left L; torso from column tc."""
    tc=L-2 if tc is None else tc
    return dict(T=T,L=L,tc=tc,sh=T+6,waist=T+12)
def build(p, back_leg, front_leg, back_arm, front_arm, pad=None, hnd=None, back_shoe=None, front_shoe=None, bow=False, extra=None):
    g=blank(); T,L,tc=p['T'],p['L'],p['tc']
    # back limbs first (darker)
    if back_arm: limb(g,back_arm,'2','2',(0,-1) if back_arm[-1][1]<back_arm[0][1] else (0,1),'5')
    leg(g,back_leg,True)
    if back_shoe: shoe(g,*back_shoe)
    torso(g,p['sh'],tc,p['waist']-p['sh']+1)
    shorts(g,p['waist']+1,tc)
    leg(g,front_leg)
    if front_shoe: shoe(g,*front_shoe)
    head(g,T,L,bow); collar(g,T+6,L)
    if pad: paddle(g,*pad)
    if hnd: handle(g,hnd)
    if front_arm: limb(g,front_arm,'3','2',(1,0),'6')
    if extra: extra(g)
    outline(g); return g

P={}
def F(name,**k): P[name]=k
# idle: ready stance, paddle up in front, both hands on it
F('idle0',p=std(3,13),back_leg=[(17,12),(22,9),(26,7)],front_leg=[(17,16),(22,19),(26,20)],
  back_arm=[(10,12),(13,15),(14,19)],front_arm=[(10,17),(13,18),(13,20)],
  pad=(7,21),hnd=[(12,21),(11,22)],back_shoe=(27,5,1),front_shoe=(27,19,1))
F('idle1',p=std(4,13),back_leg=[(18,12),(22,9),(26,7)],front_leg=[(18,16),(22,19),(26,20)],
  back_arm=[(11,12),(14,15),(15,19)],front_arm=[(11,17),(14,18),(14,20)],
  pad=(8,21),hnd=[(13,21),(12,22)],back_shoe=(27,5,1),front_shoe=(27,19,1))
# run: contact, pass, contact (other leg), pass. Paddle carried up in front.
RA=dict(front_arm=[(10,18),(13,20),(12,22)],pad=(7,23),hnd=[(11,23),(11,23)])
F('run0',p=std(3,14,12),back_leg=[(17,13),(21,10),(23,6)],front_leg=[(17,17),(21,20),(26,22)],
  back_arm=[(10,13),(13,11),(15,10)],back_shoe=(23,3,1),front_shoe=(27,21,1),**RA)
F('run1',p=std(2,14,12),back_leg=[(16,13),(20,13),(22,9)],front_leg=[(16,16),(21,17),(26,16)],
  back_arm=[(9,13),(12,12),(14,12)],back_shoe=(22,7,1),front_shoe=(27,15,1),
  front_arm=[(9,18),(12,20),(11,22)],pad=(6,23),hnd=[(10,23),(10,23)])
F('run2',p=std(3,14,12),back_leg=[(17,16),(21,20),(26,22)],front_leg=[(17,13),(21,10),(23,6)],
  back_arm=[(10,15),(12,17),(13,19)],back_shoe=(27,21,1),front_shoe=(23,3,1),**RA)
F('run3',p=std(2,14,12),back_leg=[(16,16),(21,17),(26,16)],front_leg=[(16,13),(20,13),(22,9)],
  back_arm=[(9,14),(12,15),(13,16)],back_shoe=(27,15,1),front_shoe=(22,7,1),
  front_arm=[(9,18),(12,20),(11,22)],pad=(6,23),hnd=[(10,23),(10,23)])
STAND=dict(back_leg=[(17,12),(22,9),(26,7)],front_leg=[(17,16),(22,19),(26,20)],back_shoe=(27,5,1),front_shoe=(27,19,1))
STAND4=dict(back_leg=[(18,12),(22,9),(26,7)],front_leg=[(18,16),(22,19),(26,20)],back_shoe=(27,5,1),front_shoe=(27,19,1))
F('windup',p=std(4,14,12),back_leg=[(18,13),(22,9),(26,7)],front_leg=[(18,17),(22,20),(26,21)],back_shoe=(27,5,1),front_shoe=(27,20,1),
  back_arm=[(11,17),(13,20),(13,23)],front_arm=[(11,13),(14,10),(15,8)],pad=(11,2),hnd=[(15,7),(15,7)])
F('swing',p=std(5,14,12),back_leg=[(19,13),(23,8),(26,5)],front_leg=[(19,17),(23,21),(26,22)],back_shoe=(27,3,1),front_shoe=(27,21,1),
  back_arm=[(12,13),(14,9),(15,6)],front_arm=[(12,18),(14,22),(15,24)],pad=(12,26),hnd=[(15,25),(15,25)])
F('follow',p=std(4,14,12),**STAND4,
  back_arm=[(11,13),(13,15),(14,17)],front_arm=[(11,18),(10,21),(11,14)],pad=(4,7),hnd=[(10,13),(8,11)])
F('toss',p=std(3,13),**STAND,
  front_arm=[(10,12),(13,10),(16,9)],pad=(16,3),hnd=[(17,8),(17,8)],back_arm=[(10,17),(12,20),(11,23)])
F('cheer',p=std(3,13),**STAND,
  front_arm=[(10,17),(8,20),(6,21)],pad=(0,19),hnd=[(5,21),(5,21)],back_arm=[(10,12),(13,11),(16,11)])
F('overhead_windup',p=std(3,13),**STAND,
  front_arm=[(10,12),(6,10),(3,11)],pad=(5,4),hnd=[(4,9),(4,9)],back_arm=[(10,17),(6,20),(2,22)])
F('overhead_contact',p=std(4,13),**STAND4,
  front_arm=[(11,17),(8,19),(6,20)],pad=(0,18),hnd=[(5,20),(5,20)],back_arm=[(11,12),(14,12),(16,13)])
F('reach',p=std(6,15,13),back_leg=[(20,14),(24,8),(26,3)],front_leg=[(20,18),(23,23),(26,24)],back_shoe=(27,1,1),front_shoe=(27,23,1),
  front_arm=[(13,20),(17,24),(20,25)],pad=(20,27),hnd=[(21,26),(21,26)],back_arm=[(13,15),(15,12),(16,10)])
F('scoop',p=std(7,13),back_leg=[(21,12),(23,8),(26,8)],front_leg=[(21,16),(23,21),(26,21)],back_shoe=(27,6,1),front_shoe=(27,20,1),
  front_arm=[(14,17),(18,21),(22,23)],pad=(23,22,True),hnd=[(22,23),(22,23)],back_arm=[(14,12),(17,11),(19,11)])
F('slump',p=std(4,13),back_leg=[(18,12),(22,11),(26,10)],front_leg=[(18,16),(22,17),(26,17)],back_shoe=(27,8,1),front_shoe=(27,16,1),
  front_arm=[(11,17),(14,18),(17,18)],pad=(20,16),hnd=[(18,18),(19,18)],back_arm=[(11,12),(14,11),(17,11)],bow=True)
F('thumbs_up',p=std(3,13),**STAND,
  front_arm=[(10,17),(13,20),(12,22)],back_arm=[(10,12),(13,10),(16,10)],pad=(19,7),hnd=[(17,10),(18,10)],
  extra=lambda g:(put(g,10,22,'3'),put(g,11,22,'3')))
F('flex',p=std(3,13),**STAND,
  back_arm=[(10,12),(10,8),(6,8)],front_arm=[(10,17),(10,21),(6,21)],pad=(27,24,True),hnd=[(28,23),(28,23)],
  extra=lambda g:(put(g,5,7,'33'),put(g,5,21,'33')))
F('en_garde',p=std(3,13),**STAND,
  front_arm=[(10,17),(10,23)],pad=(8,26),hnd=[(10,24),(10,25)],back_arm=[(10,12),(12,9),(14,11)])

F('scramble',p=std(8,14,12),back_leg=[(22,13),(26,10),(26,5)],front_leg=[(22,17),(23,21),(26,21)],back_shoe=(27,2,1),front_shoe=(27,20,1),
  front_arm=[(15,18),(19,21),(25,23)],back_arm=[(15,13),(16,10),(17,8)],pad=(13,2),hnd=[(17,7),(17,7)])


def head_box(g):
    for r in range(31):
        row = "".join(g[r])
        i = row.find("1FFFFF1")
        if i >= 0 and "".join(g[r + 1])[i:i + 4] in ("1FFF", "1F33"):
            return [r - 1, i]
    raise SystemExit("no head in frame")


def main():
    heads = {}
    for name, pose in P.items():
        g = build(**pose)
        ground(g)
        (ART / "frames" / f"{name}.txt").write_text(
            "\n".join("".join(r) for r in g) + "\n", "utf-8")
    order = json.loads((ART / "heads.json").read_text("utf-8"))
    for name in order["heads"]:
        rows = (ART / "frames" / f"{name}.txt").read_text("utf-8").split()
        heads[name] = head_box([list(r) for r in rows])
    order["heads"] = heads
    (ART / "heads.json").write_text(json.dumps(order, indent=1) + "\n",
                                    "utf-8")
    print(f"wrote {len(P)} frames and heads.json")


if __name__ == "__main__":
    main()
