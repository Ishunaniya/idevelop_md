#include <evs/winch_ice.h>
#include <evs/model_registry.h>
#include <evs/log.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
std::atomic<int> countdown{-1};
void* operator new(size_t n) {int v=countdown.load(); if(v>=0 && countdown.fetch_sub(1)==0){countdown.store(-1);throw std::bad_alloc();} if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(){using namespace evs;using namespace evs::winch_ice;Logger::instance().set_level(LogLevel::Off);int failures=0,successes=0,leaks=0;
for(int n=0;n<120;++n){WinchSnowConfig c;c.model_config.Model_path="/tmp/ice_slim_sdk/Y.rknn";c.model_config.output_format="yolox";c.model_config.input_width=c.model_config.input_height=640;c.model_config.warmup=false;Detector d;countdown=n;auto rc=d.initialize(c);countdown=-1;d.release();int refs=global_registry().refcount("rknn",c.model_config.Model_path);if(refs){std::cout<<"LEAK allocation="<<n<<" rc="<<rc<<" refs="<<refs<<'\n';++leaks;while(refs--)global_registry().release("rknn",c.model_config.Model_path);}if(rc)++failures;else ++successes;}
std::cout<<"failed="<<failures<<" succeeded="<<successes<<" leaks="<<leaks<<'\n';return leaks?1:0;}
