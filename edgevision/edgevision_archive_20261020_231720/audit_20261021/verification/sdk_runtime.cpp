#include <rknn_api.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
struct Context { char tag; bool outstanding=false; std::vector<float> raw; };
static Context* c(rknn_context v) { return reinterpret_cast<Context*>(v); }
extern "C" {
int rknn_init(rknn_context* out, void* model, uint32_t size, uint32_t, rknn_init_extend*) {
 if(!size)return -1; *out=reinterpret_cast<rknn_context>(new Context{*static_cast<char*>(model),false,{}});return 0;
}
int rknn_destroy(rknn_context ctx) { if(c(ctx)->outstanding)std::abort();delete c(ctx);return 0; }
int rknn_set_core_mask(rknn_context, rknn_core_mask) { return 0; }
int rknn_query(rknn_context ctx, rknn_query_cmd cmd, void* data, uint32_t) {
 if(cmd==RKNN_QUERY_IN_OUT_NUM){auto* n=static_cast<rknn_input_output_num*>(data);n->n_input=n->n_output=1;return 0;}
 auto* a=static_cast<rknn_tensor_attr*>(data); std::memset(a,0,sizeof(*a));
 if(cmd==RKNN_QUERY_INPUT_ATTR){a->n_dims=4;a->dims[0]=1;a->dims[1]=640;a->dims[2]=640;a->dims[3]=3;a->n_elems=640*640*3;a->type=RKNN_TENSOR_INT8;a->fmt=RKNN_TENSOR_NHWC;return 0;}
 if(cmd==RKNN_QUERY_OUTPUT_ATTR){a->n_dims=3;a->dims[0]=1;a->dims[1]=c(ctx)->tag=='X'?8399:8400;a->dims[2]=6;a->n_elems=a->dims[1]*6;a->type=RKNN_TENSOR_FLOAT32;a->fmt=RKNN_TENSOR_UNDEFINED;return 0;}
 return -1;
}
int rknn_inputs_set(rknn_context, uint32_t n, rknn_input* in) {return n==1&&in->size==640*640*3&&in->fmt==RKNN_TENSOR_NHWC?0:-1;}
int rknn_run(rknn_context, rknn_run_extend*) {return std::getenv("ICE_SDK_FAIL")?-1:0;}
int rknn_outputs_get(rknn_context ctx, uint32_t, rknn_output* out, rknn_output_extend*) {
 auto* v=c(ctx); if(v->outstanding)std::abort();try {v->raw.assign(8400*6,0);} catch(...) {return -1;} v->outstanding=true;
 const size_t rows[]={40*80+20,6400+20*40+30,8000+10*20+5};
 const int gx[]={20,30,5},gy[]={40,20,10},s[]={8,16,32};
 if(!std::getenv("ICE_SDK_EMPTY"))for(int i=0;i<3;++i){float* p=v->raw.data()+rows[i]*6;p[0]=(i==1?480.f:160.f)/s[i]-gx[i];p[1]=320.f/s[i]-gy[i];p[2]=p[3]=std::log(320.f/s[i]);p[4]=v->tag=='B'?5:.8f;p[5]=i==2?.7f:.9f;}
 out->buf=v->raw.data();out->size=v->raw.size()*sizeof(float);return 0;
}
int rknn_outputs_release(rknn_context ctx,uint32_t,rknn_output*) {if(!c(ctx)->outstanding)std::abort();c(ctx)->outstanding=false;return 0;}
}
