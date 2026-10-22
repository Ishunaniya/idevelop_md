#include <evs/winch_ice.h>
#include <evs/model_registry.h>
#include <evs/testing/ice_replay.h>
#include <evs/log.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
std::atomic<int> countdown{-1}; bool persistent=false, fail_persistent=false;
void* operator new(size_t n) {if(persistent)throw std::bad_alloc(); int v=countdown.load(); if(v>=0 && countdown.fetch_sub(1)==0){countdown.store(-1);persistent=fail_persistent;throw std::bad_alloc();} if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(){using namespace evs;using namespace evs::winch_ice;Logger::instance().set_level(LogLevel::Off);int failures=0,successes=0;
for(bool mode:{false,true})for(int n=0;n<110;++n){WinchSnowConfig c;c.model_config.Model_path="/tmp/ice_slim_sdk/Y.rknn";c.model_config.output_format="yolox";c.model_config.input_width=c.model_config.input_height=640;Detector d;if(d.initialize(c))return 2;
std::vector<uint8_t> pixels(200*100*3,60);for(int y=0;y<100;++y)for(int x=0;x<60;++x)for(int k=0;k<3;++k)pixels[(y*200+x)*3+k]=230;
ImageData im;im.data=pixels.data();im.width=200;im.height=100;im.size_bytes=pixels.size();WinchSnowResult r;if(d.detect(im,r)||r.alert_level!=AlertLevel::ALERT_WARN)return 3;
fail_persistent=mode;countdown=n;auto rc=d.detect(im,r);countdown=-1;persistent=false;
if(rc){++failures;std::vector<testing::IceMaskSnapshot> masks;if(r.processed||r.status!=rc||!r.objects.empty()||r.alert_level!=AlertLevel::ALERT_NONE||!testing::IceReplayAccess::copyMasks(d,masks)||!masks.empty())return 4;}else {++successes;if(!r.processed||r.objects.size()!=2||r.alert_level!=AlertLevel::ALERT_WARN)return 5;}
if(d.detect(im,r)||r.objects.size()!=2||r.alert_level!=AlertLevel::ALERT_WARN)return 6;d.release();if(global_registry().refcount("rknn",c.model_config.Model_path))return 7;}
std::cout<<"DETECT OOM SWEEP failed="<<failures<<" succeeded="<<successes<<" stale alarms/masks=0\n";return !failures||!successes;}
