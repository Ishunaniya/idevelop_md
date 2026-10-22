#include "capture.h"
namespace cap {
static int index=0;
std::vector<CamEntry> load_capture_config(const std::string&) { return {}; }
const CamEntry* select_entry(const std::vector<CamEntry>&,int) { return nullptr; }
bool GstCapture::open(const std::string&,int,int,int,bool,const char*) { w_=200;h_=100;index=0;return true; }
bool GstCapture::grab(CapFrame& out) { return grab_for(out,0); }
bool GstCapture::grab_for(CapFrame& out,int) {
 const int snow[]={20,60,20};int n=snow[index++%3];
 out.w=200;out.h=100;out.fmt=CapFmt::NV12;out.pixel_format="NV12";out.buf.assign(200*150,128);
 for(int y=0;y<100;++y)for(int x=0;x<200;++x)out.buf[y*200+x]=x<n?214:68;
 out.data=out.buf.data();return true;
}
void GstCapture::close() {}
}
