#include <evs/winch_ice.h>
#include <evs/model_registry.h>
#include <evs/log.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
std::atomic<int> countdown{-1}; bool persistent=false;
void* operator new(size_t n) {if(persistent) throw std::bad_alloc(); int v=countdown.load(); if(v>=0 && countdown.fetch_sub(1)==0){countdown.store(-1);throw std::bad_alloc();} if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(){using namespace evs;using namespace evs::winch_ice;Logger::instance().set_level(LogLevel::Off);int failures=0,successes=0,leaks=0;
WinchSnowConfig c;c.model_config.Model_path="/tmp/ice_slim_sdk/Y.rknn";c.model_config.output_format="yolox";c.model_config.input_width=c.model_config.input_height=640;c.model_config.warmup=false;Detector d;if(d.initialize(c))return 2;bool threw=false;persistent=true;try {d.release();}catch(const std::bad_alloc&){threw=true;}persistent=false;d.release();std::cout<<"release threw during persistent OOM="<<threw<<'\n';return threw?1:0;}
