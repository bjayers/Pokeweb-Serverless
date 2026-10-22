"""Independent projection checks, using host C with the same fixed-point math."""
import ctypes as c, math, subprocess, tempfile, unittest
from pathlib import Path
HERE=Path(__file__).resolve().parents[1]
class Point(c.Structure):_fields_=[('x',c.c_int32),('y',c.c_int32),('z',c.c_int32)]
class Camera(c.Structure):_fields_=[('eye',Point),('target',Point),('projection',c.c_uint32)]
class Pose(c.Structure):_fields_=[('position',Point),('sx',c.c_int32),('sy',c.c_int32)]
class Result(c.Structure):_fields_=[('axis',Point),('before',c.c_int32),('after',c.c_int32),('policy',c.c_int32)]
def xyz(p):return (p.x,p.y,p.z)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def project(pose,eye,ortho):
 n=[x/math.sqrt(dot(eye,eye)) for x in eye];r=[n[2],0,-n[0]];length=math.sqrt(dot(r,r));r=[x/length for x in r]
 up=[n[1]*r[2],n[2]*r[0]-n[0]*r[2],-n[1]*r[0]]
 # Sign of vertical axis is irrelevant to equality; compare all quad corners.
 out=[]
 for x,y in [(-1,0),(1,0),(-1,2),(1,2)]:
  point=[xyz(pose.position)[i]+r[i]*x*pose.sx+up[i]*y*pose.sy for i in range(3)]
  v=[point[i]-eye[i] for i in range(3)];depth=-dot(v,n)
  out.append((dot(v,r)/(1 if ortho else depth),dot(v,up)/(1 if ortho else depth)))
 return out
class Rendering(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();lib=Path(cls.tmp.name)/'render.so'
  subprocess.run(['cc','-shared','-fPIC','-std=c11','-Wall','-Wextra','-Werror',str(HERE/'render_math.c'),'-o',str(lib)],check=True)
  cls.lib=c.CDLL(str(lib));cls.lib.fwr_correct.argtypes=[c.POINTER(Point),c.POINTER(Point),c.POINTER(Pose),c.POINTER(Point),c.POINTER(Camera),c.c_uint,c.POINTER(Pose),c.POINTER(Result)]
  cls.lib.fwr_above_shadow.argtypes=[c.POINTER(Pose),c.POINTER(Point),c.POINTER(Camera),c.POINTER(Pose)]
  cls.lib.fwr_keep_behind.argtypes=[c.POINTER(Pose),c.POINTER(Pose),c.POINTER(Camera),c.c_int32,c.c_int32,c.POINTER(Pose)]
  cls.lib.fwr_shadow_behind.argtypes=[c.POINTER(Pose),c.POINTER(Point),c.POINTER(Camera),c.POINTER(Pose)]
  cls.lib.fwr_shadow_on_feet.argtypes=[c.POINTER(Pose),c.POINTER(Point),c.POINTER(Point),c.POINTER(Camera),c.POINTER(Pose)]
  cls.lib.fwr_player_in_front.argtypes=[c.POINTER(Pose),c.POINTER(Pose),c.POINTER(Camera),Point,c.c_int32,c.c_int32,c.POINTER(Pose),c.POINTER(c.c_int32)]
  cls.lib.fwr_head_position.argtypes=[c.POINTER(Point),c.POINTER(Camera),c.c_int32,c.POINTER(Point)]
  cls.lib.fwr_emote_place.argtypes=[c.POINTER(Pose),c.POINTER(Point),c.POINTER(Camera),c.POINTER(Pose)]
  cls.lib.fwr_emote_place.restype=c.c_int
  cls.lib.fwr_cover_feet.argtypes=[c.POINTER(Pose),c.POINTER(Pose),c.POINTER(Pose),c.POINTER(Camera),c.POINTER(Pose)]
  cls.lib.fwr_box_advance_limit.argtypes=[c.POINTER(Pose),c.POINTER(Camera),c.POINTER(Point)]
  cls.lib.fwr_box_advance_limit.restype=c.c_int32
  cls.lib.fwr_cap_advance.argtypes=[c.POINTER(Pose),c.POINTER(Pose),c.POINTER(Camera),c.c_int32,c.POINTER(Pose)]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def run_case(self,world,pose,cam,large=1):
  out=Pose();res=Result();p=Point();applied=self.lib.fwr_correct(c.byref(world),c.byref(p),c.byref(pose),c.byref(p),c.byref(cam),large,c.byref(out),c.byref(res));return applied,out,res
 def test_projection_and_order(self):
  cases=0
  for size in (8192,16384):
   for projection in (0,1,2):
    for yaw in range(0,360,15):
     for pitch in (20,35,45,52.58,65,75):
      y,p=map(math.radians,(yaw,pitch));eye=tuple(int(v*240*4096)for v in (math.sin(y)*math.cos(p),math.sin(p),math.cos(y)*math.cos(p)))
      cam=Camera(Point(*eye),Point(),projection)
      world=Point(int(math.cos(y)*65536),0,int(-math.sin(y)*65536))
      for bob in (0,6144):
       pose=Pose(Point(world.x,bob,world.z),size,size);applied,out,res=self.run_case(world,pose,cam,size==16384)
       self.assertTrue(applied);self.assertLessEqual(res.after,-508)
       before=project(pose,eye,projection==2);after=project(out,eye,projection==2)
       # Ortho: <1/1000 world unit of projected drift. Perspective: <0.002
       # pixels at focal length 256, allowing fx16 quad-size quantization.
       error=max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b))
       self.assertLess(error,4 if projection==2 else 0.002/256)
       cases+=1
  self.assertEqual(cases,1728)
 def test_static_model_depth_and_projection(self):
  for projection in (0,1,2):
   for yaw in range(0,360,15):
    angle=math.radians(yaw);pitch=math.radians(52)
    n=(math.sin(angle)*math.cos(pitch),math.sin(pitch),math.cos(angle)*math.cos(pitch))
    right=(math.cos(angle),0,-math.sin(angle));up=(n[1]*right[2],n[2]*right[0]-n[0]*right[2],-n[1]*right[0])
    eye=tuple(int(v*240*4096) for v in n);cam=Camera(Point(*eye),Point(),projection)
    body=Pose(Point(),8192,8192);candidate=Pose(Point(*(int(v*12*4096) for v in n)),8192,8192)
    # Build an independent world-space volume in front of the lower sprite.
    def box(side=0,near=6,far=9):
     return (Point*8)(*(Point(*(int((right[k]*(side+(8 if i&1 else -8))+up[k]*(8 if i&2 else 0)+n[k]*(far if i&4 else near))*4096) for k in range(3))) for i in range(8)))
    corners=box();limit=self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),corners)
    self.assertAlmostEqual(limit,2*4096,delta=64)
    out=Pose();self.assertTrue(self.lib.fwr_cap_advance(c.byref(body),c.byref(candidate),c.byref(cam),limit,c.byref(out)))
    self.assertLessEqual(dot(xyz(out.position),n),min(dot(xyz(p),n) for p in corners)-4*4096+64)
    # The depth cap must preserve the already-adjusted image, including size.
    # Use the production translation first so the input lies on its eye ray.
    baseline=Pose(Point(),8192,8192)
    raised=Pose();depth=c.c_int32()
    follower=Pose(Point(*(int(v*12*4096) for v in n)),8192,8192)
    normalized=Result();ignored=Pose();origin=Point()
    self.lib.fwr_correct(c.byref(origin),c.byref(origin),c.byref(baseline),c.byref(origin),c.byref(cam),0,c.byref(ignored),c.byref(normalized))
    self.lib.fwr_player_in_front(c.byref(follower),c.byref(baseline),c.byref(cam),normalized.axis,512,32*4096,c.byref(raised),c.byref(depth))
    self.assertTrue(self.lib.fwr_cap_advance(c.byref(baseline),c.byref(raised),c.byref(cam),limit,c.byref(out)))
    before=project(raised,eye,projection==2);after=project(out,eye,projection==2)
    error=max(abs(x-y) for a,b in zip(before,after) for x,y in zip(a,b))
    # Normalizing the axis and applying two fx20.12 translations accumulate
    # less than 0.005 world units of orthographic drift (1/200 source pixel).
    self.assertLess(error,20 if projection==2 else 0.004/256)
    self.assertEqual(self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),box(side=64)),32*4096)
    self.assertEqual(self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),box(near=-6,far=-3)),32*4096)
    self.assertEqual(self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),box(near=-1,far=3)),0)
 def test_furniture_gap_survives_native_depth_quantization(self):
  # Captured field camera: near=1, far=1024. The old 1/8-unit clearance
  # collapsed onto the trashcan's Z value, despite passing float geometry.
  eye=(2195456,1302017,47460865);target=(2195456,689441,46825472)
  n=[eye[i]-target[i] for i in range(3)];length=math.sqrt(dot(n,n));n=[v/length for v in n]
  body=Pose(Point(2195456,585395,46825472),8192,8192);cam=Camera(Point(*eye),Point(*target),0)
  origin=(2195456,589824,46891008)
  lo=(-7541*4,965*4,-9452*4);extent=(16443*4,13428*4,16200*4)
  corners=(Point*8)(*(Point(*(origin[k]+lo[k]+(extent[k] if i&(1<<k) else 0) for k in range(3))) for i in range(8)))
  rear=min(dot(xyz(p),n) for p in corners);eye_depth=dot(eye,n)
  def z(depth):
   distance=(eye_depth-depth)/4096
   return math.trunc((4104*distance-8200)/distance*4)+0x3fff
  self.assertEqual(z(rear-512),z(rear))
  limit=self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),corners)
  self.assertAlmostEqual(limit,rear-dot(xyz(body.position),n)-4*4096,delta=64)
  self.assertGreaterEqual(z(dot(xyz(body.position),n)+limit),z(rear)+2)
  # Nearby props with insufficient room retain the original sprite depth.
  shifted=(Point*8)(*(Point(*(xyz(p)[k]-int(n[k]*4*4096) for k in range(3))) for p in corners))
  self.assertEqual(self.lib.fwr_box_advance_limit(c.byref(body),c.byref(cam),shifted),0)
 def test_reject_bad_camera_and_discontinuities(self):
  for cam in [Camera(Point(),Point(),0),Camera(Point(0,1000000,1000000),Point(),3),Camera(Point(2147483647,0,0),Point(-2147483648,0,0),0)]:
   pose=Pose(Point(65536,6144,0),8192,8192);applied,out,res=self.run_case(Point(65536,0,0),pose,cam)
   self.assertEqual(applied,0);self.assertEqual(bytes(pose),bytes(out))
 def test_grass_covers_owner_without_covering_nearer_body(self):
  for projection in (0,1,2):
   for yaw in range(0,360,15):
    angle=math.radians(yaw);pitch=math.radians(52.58)
    eye=tuple(int(v*240*4096) for v in (math.sin(angle)*math.cos(pitch),math.sin(pitch),math.cos(angle)*math.cos(pitch)))
    n=[v/math.sqrt(dot(eye,eye)) for v in eye];cam=Camera(Point(*eye),Point(),projection)
    def at_depth(depth):return Pose(Point(*(int(v*depth) for v in n)),8192,8192)
    owner=at_depth(0)
    for gap in (-12*4096,512,2*4096,12*4096,20*4096):
     other=at_depth(gap)
     for initial in (-8*4096,2*4096,8*4096,24*4096):
      grass=at_depth(initial);out=Pose()
      self.lib.fwr_cover_feet(c.byref(grass),c.byref(owner),c.byref(other),c.byref(cam),c.byref(out))
      depth=dot(xyz(out.position),n)
      self.assertGreaterEqual(depth,min(4*4096,gap/3) - 64 if gap>0 else 4*4096-64)
      if gap>0:self.assertLessEqual(depth,gap-min(4*4096,gap/3)+64)
      before=project(grass,eye,projection==2);after=project(out,eye,projection==2)
      error=max(abs(a-b) for p,q in zip(before,after) for a,b in zip(p,q))
      self.assertLess(error,64 if projection==2 else .002/256)
      again=Pose();self.assertFalse(self.lib.fwr_cover_feet(c.byref(out),c.byref(owner),c.byref(other),c.byref(cam),c.byref(again)))
      self.assertEqual(bytes(out),bytes(again))
  # Grass's native Y=9 contributes to camera-normal depth. Do not push an
  # already-correct tile forward merely because its horizontal Z is behind.
  cam=Camera(Point(0,760739,581835),Point(),0)
  grass=Pose(Point(0,9*4096,-2*4096),8192,8192);owner=Pose(Point(0,-6484,0),8192,8192)
  out=Pose();self.assertFalse(self.lib.fwr_cover_feet(c.byref(grass),c.byref(owner),None,c.byref(cam),c.byref(out)))
  self.assertEqual(bytes(out),bytes(grass))
 def test_emote_camera_plane_attachment(self):
  base=Point(-24*4096,6*4096,7*4096)
  for yaw in range(0,360,15):
   for pitch in (20,35,45,52.58,65,75):
    y,p=map(math.radians,(yaw,pitch));eye=tuple(int(v*240*4096) for v in (math.sin(y)*math.cos(p),math.sin(p),math.cos(y)*math.cos(p)))
    cam=Camera(Point(*eye),Point(),0)
    for height in (34*4096,66*4096):
     head=Point();self.assertTrue(self.lib.fwr_head_position(c.byref(base),c.byref(cam),height,c.byref(head)))
     diff=(head.x-base.x,head.y-base.y,head.z-base.z)
     self.assertGreater(diff[1],0)
     # Unit-vector rounding stays below 1/16 of a world pixel, even for a
     # 64px quad. Native rasterization rounds more coarsely than this.
     self.assertLess(abs(sum(diff[i]*eye[i] for i in range(3)))/math.sqrt(sum(v*v for v in eye)),256)
     self.assertLess(abs(diff[0]*eye[2]-diff[2]*eye[0])/math.hypot(eye[0],eye[2]),256)
  for eye,height in ((Point(),34*4096),(Point(0,240*4096,0),34*4096),(Point(0,240*4096,240*4096),-1)):
   cam=Camera(eye,Point(),0);head=Point();self.assertFalse(self.lib.fwr_head_position(c.byref(base),c.byref(cam),height,c.byref(head)))
 def test_emote_clears_overhang_without_covering_the_player(self):
  eye=(0,760739,581835);cam=Camera(Point(*eye),Point(),0)
  bubble=Pose(Point(),8192,8192);placed=Pose()
  self.assertTrue(self.lib.fwr_emote_place(c.byref(bubble),None,c.byref(cam),c.byref(placed)))
  n=[v/math.sqrt(dot(eye,eye)) for v in eye]
  closer=dot(xyz(placed.position),n)-dot(xyz(bubble.position),n)
  self.assertGreater(closer,4*65536)
  before=project(bubble,eye,False);after=project(placed,eye,False)
  self.assertLess(max(abs(x-y) for a,b in zip(before,after) for x,y in zip(a,b)),0.002/256)
  player=Point(*(int(v*24*4096) for v in n))
  held=Pose();self.assertTrue(self.lib.fwr_emote_place(c.byref(bubble),c.byref(player),c.byref(cam),c.byref(held)))
  depth=dot((held.position.x-player.x,held.position.y-player.y,held.position.z-player.z),n)
  self.assertLessEqual(depth,-4096+256)
  after=project(held,eye,False)
  self.assertLess(max(abs(x-y) for a,b in zip(before,after) for x,y in zip(a,b)),0.002/256)
  # Beside the follower: the canopy clearance stays, the bubble is not pulled back.
  right=(n[2],0,-n[0]);length=math.sqrt(dot(right,right));right=[v/length for v in right]
  beside=Point(*(int(right[i]*2*65536) for i in range(3)))
  side=Pose();self.assertTrue(self.lib.fwr_emote_place(c.byref(bubble),c.byref(beside),c.byref(cam),c.byref(side)))
  self.assertGreater(dot(xyz(side.position),n),4*65536)
 def test_stair_policy_does_not_depend_on_bob(self):
  cam=Camera(Point(0,760739,581835),Point(),0)
  # Side-and-above used to return policy 0. It now clamps behind, independent of bob.
  for y,policy in [(65536,-1),(-32740,-2)]:
   for bob in (0,6144):
    world=Point(65536,y,0);pose=Pose(Point(57344,y+bob,-8192),16384,16384)
    applied,out,res=self.run_case(world,pose,cam)
    self.assertEqual(res.policy,policy)
    if policy==-1:
     self.assertLessEqual(res.after,-4096+64)
     before=project(pose,(0,760739,581835),False);after=project(out,(0,760739,581835),False)
     self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),0.002/256)
     player=Point();again=Pose()
     self.assertFalse(self.lib.fwr_correct(c.byref(world),c.byref(player),c.byref(out),c.byref(player),c.byref(cam),1,c.byref(again),c.byref(res)))
 def test_south_stair_clamps_follower_behind_player(self):
  # One tile north and one tile up: the old early-out left this at policy 0.
  cam=Camera(Point(0,760739,581835),Point(),0)
  world=Point(0,65536,-65536)
  pose=Pose(Point(0,65536,-65536),16384,16384)
  for flags in (0,1):
   applied,out,res=self.run_case(world,pose,cam,flags)
   self.assertEqual(res.policy,-1)
   self.assertLessEqual(res.after,-4096+64)
   before=project(pose,(0,760739,581835),False);after=project(out,(0,760739,581835),False)
   self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),0.002/256)
 def test_north_follower_keeps_previous_foreground_depth(self):
  eye=(0,760739,581835);cam=Camera(Point(*eye),Point(),0)
  world=Point(0,0,65536)
  # North artwork is tucked two Y units and stays on the shadow's ground plane.
  # Restore the previous *depth*, not its pixels. The old -7 Z shift is gone.
  old=Pose(Point(0,-6*4096,65536-2*4096),8192,8192)
  current=Pose(Point(0,-8*4096,65536-2*4096),8192,8192)
  _,_,old_result=self.run_case(world,old,cam,0)
  applied,out,result=self.run_case(world,current,cam,2)
  self.assertTrue(applied);self.assertEqual(result.policy,1)
  self.assertGreaterEqual(result.after,old_result.before)
  self.assertLess(result.after-old_result.before,128)
  before=project(current,eye,False);after=project(out,eye,False)
  self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),0.002/256)
  # Lateral and unequal-height policies do not opt into the north repair.
  _,_,side=self.run_case(world,current,cam,0)
  self.assertLess(side.after,result.after)
  upper=Point(0,65536,65536)
  _,_,stairs=self.run_case(upper,current,cam,3)
  _,_,prior_stairs=self.run_case(upper,current,cam,1)
  self.assertEqual((stairs.policy,stairs.after),(prior_stairs.policy,prior_stairs.after))
 def test_billboard_stays_in_front_of_shadow_without_moving_on_screen(self):
  # The supplied mounted Reuniclus state has its player shadow at this world
  # point and the submitted mount below that ground plane.
  ground=Point(2916352,65551,49381376)
  eye=Point(2916352,842674,49963211);target=Point(2916352,81935,49381376)
  pose=Pose(Point(2916352,59067-5*4096,49381376),8192,8192)
  for projection in (0,1,2):
   cam=Camera(eye,target,projection);out=Pose()
   self.assertEqual(self.lib.fwr_above_shadow(c.byref(pose),c.byref(ground),c.byref(cam),c.byref(out)),1)
   axis=[eye.x-target.x,eye.y-target.y,eye.z-target.z];n=math.sqrt(dot(axis,axis));axis=[x/n for x in axis]
   offset=[xyz(out.position)[i]-xyz(ground)[i] for i in range(3)]
   self.assertGreaterEqual(dot(offset,axis),6*4096-32)
   local_eye=tuple(x-y for x,y in zip(xyz(eye),xyz(target)))
   local_pose=Pose(Point(*(x-y for x,y in zip(xyz(pose.position),xyz(target)))),pose.sx,pose.sy)
   local_out=Pose(Point(*(x-y for x,y in zip(xyz(out.position),xyz(target)))),out.sx,out.sy)
   before=project(local_pose,local_eye,projection==2);after=project(local_out,local_eye,projection==2)
   self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),4 if projection==2 else 0.002/256)
   again=Pose();self.assertEqual(self.lib.fwr_above_shadow(c.byref(out),c.byref(ground),c.byref(cam),c.byref(again)),0)
   self.assertEqual(bytes(out),bytes(again))
 def test_south_follower_shadow_clearance_cannot_reverse_player_priority(self):
  # walkdown.mln: Arceus is one tile north of the player. Its -5 Y artwork
  # offset put it safely behind, then the +6 shadow clearance put it in front.
  # Rebase the saved world coordinates so fixed-point arithmetic is identical.
  world=Point(0,0,-16*4096);player=Point();player_draw=Point(0,-6484,0)
  artwork=Pose(Point(0,-340-5*4096,-12*4096),16384,16384)
  ground=Point(0,0,-10*4096)
  eye=(0,760739,581835)
  for projection in (0,1,2):
   cam=Camera(Point(*eye),Point(),projection)
   ordered=Pose();result=Result()
   self.lib.fwr_correct(c.byref(world),c.byref(player),c.byref(artwork),c.byref(player_draw),c.byref(cam),1,c.byref(ordered),c.byref(result))
   self.assertEqual(result.policy,-1)
   raised=Pose();self.assertEqual(self.lib.fwr_above_shadow(c.byref(ordered),c.byref(ground),c.byref(cam),c.byref(raised)),1)
   direction=xyz(result.axis)
   depth=lambda pose:math.trunc(sum((a-b)*n for a,b,n in zip(xyz(raised.position),xyz(pose.position),direction))/4096)
   player_pose=Pose(player_draw,8192,8192)
   self.assertGreater(depth(player_pose),0) # The supplied frame's regression.
   foreground=Pose();final=c.c_int32()
   self.assertEqual(self.lib.fwr_player_in_front(c.byref(raised),c.byref(player_pose),c.byref(cam),result.axis,12*4096,32*4096,c.byref(foreground),c.byref(final)),1)
   self.assertLessEqual(final.value,-12*4096)
   self.assertEqual(final.value,depth(foreground))
   before=project(player_pose,eye,projection==2);after=project(foreground,eye,projection==2)
   self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),4 if projection==2 else 0.002/256)
   unchanged=Pose();self.assertEqual(self.lib.fwr_player_in_front(c.byref(raised),c.byref(foreground),c.byref(cam),result.axis,12*4096,32*4096,c.byref(unchanged),c.byref(final)),0)
   self.assertEqual(bytes(foreground),bytes(unchanged))
   # The draw path no longer shoves the player. The follower quad is tucked
   # one unit behind the player after the shadow push, and the player stays put.
   behind=Pose();player_pose=Pose(player_draw,8192,8192)
   self.assertEqual(self.lib.fwr_keep_behind(c.byref(raised),c.byref(player_pose),c.byref(cam),4096,16*4096,c.byref(behind)),1)
   kept=math.trunc(sum((a-b)*n for a,b,n in zip(xyz(behind.position),xyz(player_pose.position),direction))/4096)
   self.assertLessEqual(kept,-4096)
   self.assertGreater(kept,-4*4096)
   held=Pose();self.assertEqual(self.lib.fwr_keep_behind(c.byref(behind),c.byref(player_pose),c.byref(cam),4096,16*4096,c.byref(held)),0)
   self.assertEqual(bytes(behind),bytes(held))
   screen_before=project(raised,eye,projection==2);screen_after=project(behind,eye,projection==2)
   self.assertLess(max(abs(x-y)for a,b in zip(screen_before,screen_after)for x,y in zip(a,b)),4 if projection==2 else 0.002/256)
   # The same tuck must not cross a wall when the pose starts far in front.
   far=Pose(Point(*(int(v*20) for v in direction)),16384,16384)
   capped=Pose()
   self.assertEqual(self.lib.fwr_keep_behind(c.byref(far),c.byref(player_pose),c.byref(cam),4096,16*4096,c.byref(capped)),1)
   moved=math.trunc(sum((a-b)*n for a,b,n in zip(xyz(capped.position),xyz(far.position),direction))/4096)
   self.assertGreaterEqual(moved,-16*4096-128)
   self.assertLess(moved,-15*4096)
   # The shadow receives that same eye-ray bias, so it stays behind the sprite
   # without leaving its projected ground position.
   shade=Pose(Point(*(a+b for a,b in zip(xyz(behind.position),direction))),4096,4096);tucked=Pose()
   self.assertEqual(self.lib.fwr_shadow_behind(c.byref(shade),c.byref(behind.position),c.byref(cam),c.byref(tucked)),1)
   shadow_depth=math.trunc(sum((a-b)*n for a,b,n in zip(xyz(tucked.position),xyz(behind.position),direction))/4096)
   self.assertLessEqual(shadow_depth,-4096+64)
   shade_before=project(shade,eye,projection==2);shade_after=project(tucked,eye,projection==2)
   self.assertLess(max(abs(x-y)for a,b in zip(shade_before,shade_after)for x,y in zip(a,b)),4 if projection==2 else 0.002/256)
   again=Pose();self.assertEqual(self.lib.fwr_shadow_behind(c.byref(tucked),c.byref(behind.position),c.byref(cam),c.byref(again)),0)
 def test_shadow_stays_on_its_native_ground(self):
  # The oval is the ground sample. A sprite drawn a step low must not drag
  # the shadow with it; the bias keeps the oval's projected image.
  eye=(0,760739,581835);cam=Camera(Point(*eye),Point(),0)
  feet=Point(0,65536,-65536);sprite=Point(0,65536,-65536)
  feet_screen=project(Pose(feet,4096,4096),eye,False)
  for dz,dy in ((0,-65536),(0,65536),(6*4096,0),(0,4096)):
   shade=Pose(Point(feet.x,feet.y+dy,feet.z+dz),4096,4096);out=Pose()
   code=self.lib.fwr_shadow_on_feet(c.byref(shade),c.byref(feet),c.byref(sprite),c.byref(cam),c.byref(out))
   self.assertIn(code,(0,1))
   before=project(shade,eye,False);after=project(out,eye,False)
   self.assertLess(max(abs(x-y)for a,b in zip(before,after)for x,y in zip(a,b)),0.002/256)
   if abs(dy)>2*4096 or abs(dz)>2*4096:
    # Projection units are 1/256 of a pixel at this camera. A step away from
    # the feet must stay visibly off the feet; the bias does not pull it there.
    self.assertGreater(max(abs(x-y)for a,b in zip(feet_screen,after)for x,y in zip(a,b)),1/256)
 def test_close_range_depth_follows_the_ease(self):
  # The close pose and the swap crossing need a deeper eye-ray tuck than the
  # one-unit default. That tuck blends back to the default on the same
  # smoothstep the spacing uses, and a zoomed camera scales both ends.
  self.lib.fwr_view_distance.argtypes=[c.POINTER(Camera)]
  self.lib.fwr_view_distance.restype=c.c_uint32
  self.lib.fwr_sort_margin.argtypes=[c.POINTER(Camera),c.c_uint32]
  self.lib.fwr_sort_margin.restype=c.c_int32
  self.lib.fwr_clear_front.argtypes=[c.POINTER(Camera)]
  self.lib.fwr_clear_front.restype=c.c_int32
  near=(0,760739,581835)
  span=math.sqrt(sum(v*v for v in near))
  exact=tuple(int(v*240*4096/span) for v in near)
  packaged=(0,200*4096,140*4096)
  zoom=tuple(v*2 for v in near)
  def camera(eye,projection=0):return Camera(Point(*eye),Point(),projection)
  for eye in (near,exact,packaged):
   view=camera(eye)
   self.assertLessEqual(self.lib.fwr_view_distance(c.byref(view)),256*4096)
   self.assertEqual(self.lib.fwr_sort_margin(c.byref(view),65536),4096)
   self.assertEqual(self.lib.fwr_sort_margin(c.byref(view),70000),4096)
   self.assertEqual(self.lib.fwr_sort_margin(c.byref(view),0),8*4096)
   self.assertEqual(self.lib.fwr_clear_front(c.byref(view)),4*4096)
  far=camera(zoom)
  self.assertGreater(self.lib.fwr_view_distance(c.byref(far)),256*4096)
  zoom_idle=self.lib.fwr_sort_margin(c.byref(far),65536)
  zoom_strong=self.lib.fwr_sort_margin(c.byref(far),0)
  self.assertGreater(zoom_idle,4096)
  self.assertLessEqual(zoom_idle,65536)
  self.assertGreater(zoom_strong,8*4096)
  self.assertLessEqual(zoom_strong,65536)
  self.assertGreater(zoom_strong,zoom_idle)
  self.assertGreater(self.lib.fwr_clear_front(c.byref(far)),4*4096)
  self.assertLessEqual(self.lib.fwr_clear_front(c.byref(far)),32768)
  for eye in (near,zoom):
   view=camera(eye)
   previous=None
   for step in range(9):
    margin=self.lib.fwr_sort_margin(c.byref(view),65536*step//8)
    if previous is not None:self.assertLessEqual(margin,previous)
    previous=margin
   self.assertEqual(previous,self.lib.fwr_sort_margin(c.byref(view),65536))
  def depth(pose,player,axis):
   return math.trunc(sum((a-b)*n for a,b,n in zip(xyz(pose.position),xyz(player.position),axis))/4096)
  # North of the player is behind this camera. South is toward it.
  separations=(('five_eighths',-5*65536//8),('quarter',-65536//4),('cross',0),('brush',2048),('front',65536))
  for name,eye in (('near',near),('zoom',zoom)):
   for projection in (0,2):
    view=camera(eye,projection)
    idle=self.lib.fwr_sort_margin(c.byref(view),65536)
    close=self.lib.fwr_sort_margin(c.byref(view),0)
    clear=self.lib.fwr_clear_front(c.byref(view))
    for label,z in separations:
     world=Point(0,0,z);origin=Point();pose=Pose(Point(0,0,z),8192,8192)
     ordered=Pose();result=Result()
     self.lib.fwr_correct(c.byref(world),c.byref(origin),c.byref(pose),c.byref(origin),c.byref(view),0,c.byref(ordered),c.byref(result))
     player=Pose(origin,8192,8192);axis=xyz(result.axis);rel=depth(ordered,player,axis)
     def settle(ease_s,margin):
      blend=ease_s<65536
      tuck=result.policy==-1 or (blend and result.policy==1 and rel<=clear)
      out=Pose();moved=0
      if tuck:
       moved=self.lib.fwr_keep_behind(c.byref(ordered),c.byref(player),c.byref(view),margin,16*4096,c.byref(out))
       if not moved:out=ordered
      else:out=ordered
      return tuck,depth(out,player,axis),out
     tucked,final,out=settle(0,close)
     _,idle_final,_=settle(65536,idle)
     if label=='front':
      self.assertEqual(result.policy,1,name)
      self.assertGreater(rel,clear)
      self.assertFalse(tucked)
      self.assertGreater(final,clear)
      self.assertEqual(final,idle_final)
     else:
      self.assertTrue(tucked,(name,label,result.policy,rel,clear))
      self.assertLessEqual(final,-close+256,(name,label,final,close))
      self.assertGreater(final,-close-1024,(name,label,final,close))
      self.assertLess(final,idle_final,(name,label,final,idle_final))
      before=project(ordered,eye,projection==2);after=project(out,eye,projection==2)
      error=max(abs(a-b)for p,q in zip(before,after)for a,b in zip(p,q))
      self.assertLess(error,4 if projection==2 else 0.002/256)
     if name=='near' and projection==0 and label in ('five_eighths','quarter','cross'):
      # Idle is the historical one-unit tuck, including its no-op when the
      # follower is already that far behind.
      direct=Pose()
      moved=self.lib.fwr_keep_behind(c.byref(ordered),c.byref(player),c.byref(view),4096,16*4096,c.byref(direct))
      self.assertEqual(idle_final,depth(direct if moved else ordered,player,axis))
     if name=='zoom' and projection==0 and label=='five_eighths':
      dist=self.lib.fwr_view_distance(c.byref(view))/4096
      steps=lambda view_depth:32800*(abs(view_depth)/4096)/(dist*dist)
      self.assertLess(steps(rel),1.5)
      self.assertGreaterEqual(steps(final),2)
  # The fresh-load rest is already behind the strong target on a normal
  # camera, so the blend and the idle path submit the same pose. On the
  # zoomed camera the late blend has fallen back under that gap too.
  rest=Pose(Point(0,0,-90112),8192,8192);origin=Point()
  for eye in (near,zoom):
   view=camera(eye);ordered=Pose();result=Result()
   self.lib.fwr_correct(c.byref(Point(0,0,-90112)),c.byref(origin),c.byref(rest),c.byref(origin),c.byref(view),0,c.byref(ordered),c.byref(result))
   player=Pose(origin,8192,8192);axis=xyz(result.axis)
   late=self.lib.fwr_sort_margin(c.byref(view),65536*7//8)
   done=self.lib.fwr_sort_margin(c.byref(view),65536)
   self.assertLessEqual(done,late)
   for margin in (late,done):
    out=Pose()
    self.assertEqual(self.lib.fwr_keep_behind(c.byref(ordered),c.byref(player),c.byref(view),margin,16*4096,c.byref(out)),0)
    self.assertEqual(bytes(ordered),bytes(out))
if __name__=='__main__':unittest.main()
