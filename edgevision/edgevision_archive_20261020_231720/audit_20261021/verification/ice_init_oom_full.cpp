#include <evs/winch_ice.h>
#include <evs/model_registry.h>
#include <evs/log.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
std::atomic<int> countdown{-1}; bool persistent=false, fail_persistent=false;
void* operator new(size_t n) {if(persistent)throw std::bad_alloc(); int v=countdown.load(); if(v>=0 && countdown.fetch_sub(1)==0){countdown.store(-1);persistent=fail_persistent;throw std::bad_alloc();} if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(){using namespace evs;using namespace evs::winch_ice;Logger::instance().set_level(LogLevel::Off);int failures=0,successes=0,leaks=0;
for(bool shared : {false,true}) for(bool warmup : {false,true}) for(bool mode : {false,true}) for(int n=0;n<80;++n){
WinchSnowConfig c;c.model_config.Model_path="/tmp/ice_slim_sdk/Y.rknn";c.model_config.output_format="yolox";c.model_config.input_width=c.model_config.input_height=640;c.model_config.warmup=warmup;
Detector keeper;if(shared && keeper.initialize(c))return 3;Detector d;fail_persistent=mode;countdown=n;auto rc=d.initialize(c);countdown=-1;persistent=false;d.release();int refs=global_registry().refcount("rknn",c.model_config.Model_path);if(refs!=int(shared)){std::cout<<"LEAK allocation="<<n<<" rc="<<rc<<" refs="<<refs<<'\n';++leaks;}
if(rc){++failures; if(d.initialize(c))return 4;d.release();}else ++successes;
keeper.release();if(global_registry().refcount("rknn",c.model_config.Model_path))return 5;}
std::cout<<"INITIALIZE OOM SWEEP failed="<<failures<<" succeeded="<<successes<<" leaks="<<leaks<<'\n';return leaks?1:0;}
