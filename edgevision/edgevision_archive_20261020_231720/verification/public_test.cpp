#include <evs/winch_ice.h>
#include <evs/testing/ice_replay.h>
#include <evs/status.h>
#include <evs/log.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
std::atomic<int> countdown{-1};
void* operator new(size_t n){int v=countdown.load();if(v>=0&&countdown.fetch_sub(1)==0){countdown.store(-1);throw std::bad_alloc();}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace evs; using namespace evs::winch_ice;
int failed=0,passed=0;
#define C(x) do { if(x)++passed;else {++failed;std::cerr<<__LINE__<<": " #x "\n";} }while(0)
int main(){
 Logger::instance().set_level(LogLevel::Off);
 for(char tag:{'Y','X','B'}){std::ofstream file(std::string("/tmp/ice_slim_sdk/")+tag+".rknn");file<<tag;}
 WinchSnowConfig cfg;cfg.model_config.Model_path="/tmp/ice_slim_sdk/Y.rknn";cfg.model_config.input_width=cfg.model_config.input_height=640;cfg.model_config.output_format="yolox";
 Detector d; C(d.initialize(cfg)==0); C(d.initialize(cfg)==0);
 std::vector<uint8_t> pixels(200*100*3);
 ImageData im;im.data=pixels.data();im.width=200;im.height=100;im.size_bytes=pixels.size();im.timestamp_ms=123;
 auto fill=[&](int n){for(int y=0;y<100;++y)for(int x=0;x<200;++x)for(int k=0;k<3;++k)pixels[(y*200+x)*3+k]=x<n?230:60;};
 WinchSnowResult r;
 for(int n:{20,60,20,50}){fill(n);C(d.detect(im,r)==0);C(r.processed&&r.objects.size()==2);if(r.objects.size()!=2)return 1;C(r.objects[0].valid&&r.objects[1].valid);C(std::abs(r.objects[0].ice_ratio-float(n)/100)<1e-6f&&r.objects[0].surface_pixels==10000);C(r.objects[1].ice_ratio==0);C((r.alert_level==AlertLevel::ALERT_WARN)==(n>=50));}
 std::vector<testing::IceMaskSnapshot> masks;C(testing::IceReplayAccess::copyMasks(d,masks)&&masks.size()==2);
 auto bad=im;bad.size_bytes=1;C(d.detect(bad,r)!=0&&!r.processed&&r.objects.empty()&&r.alert_level==AlertLevel::ALERT_NONE);C(testing::IceReplayAccess::copyMasks(d,masks)&&masks.empty());
 for(const char* flag:{"ICE_SDK_FAIL","ICE_SDK_EMPTY"}){setenv(flag,"1",1);auto rc=d.detect(im,r);C((rc!=0)==(std::string(flag)=="ICE_SDK_FAIL"));C(r.objects.empty()&&r.alert_level==AlertLevel::ALERT_NONE);C(testing::IceReplayAccess::copyMasks(d,masks)&&masks.empty());unsetenv(flag);C(d.detect(im,r)==0&&r.objects.size()==2);}
 int rejected=0,applied=0;
 for(int failat=0;failat<60;++failat){Detector task;C(task.initialize(cfg)==0);auto upd=cfg.detect_config;upd.area_ratio=.8f;countdown.store(failat);auto status=task.updateConfig(upd);countdown.store(-1);C(status==0||status==with_module(8,uint32_t(EvsStatus::MemoryAllocation)));fill(70);C(task.detect(im,r)==0&&r.objects.size()==2);C(r.alert_level==(status?AlertLevel::ALERT_WARN:AlertLevel::ALERT_NONE));if(status)++rejected;else ++applied;}
 C(rejected&&applied);
 for(char tag:{'X','B'}){Detector invalid;auto other=cfg;other.model_config.Model_path=std::string("/tmp/ice_slim_sdk/")+tag+".rknn";auto rc=invalid.initialize(other);if(tag=='X')C(rc!=0);else {C(rc==0);C(invalid.detect(im,r)!=0&&r.objects.empty()&&!r.processed);C(testing::IceReplayAccess::copyMasks(invalid,masks)&&masks.empty());}}
 d.release();d.release();C(d.detect(im,r)==with_module(8,uint32_t(EvsStatus::NotInitialized))&&r.objects.empty());
 std::cout<<"PUBLIC SDK SIMULATION PASS="<<passed<<" FAIL="<<failed<<"\n";return failed?1:0;
}
