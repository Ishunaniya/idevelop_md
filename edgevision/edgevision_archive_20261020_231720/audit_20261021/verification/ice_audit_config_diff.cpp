#include <evs/task_config.h>
#include <evs/status.h>
#include <iostream>
int main(int argc,char**argv){for(int i=1;i<argc;++i){evs::TaskConfig c;uint32_t err=0;bool ok=evs::load_task_config(argv[i],c,err);std::cout<<argv[i]<<' '<<ok<<' '<<err<<' '<<c.module_id<<' '<<c.backend<<' '<<c.Model_path<<' '<<c.input_width<<' '<<c.input_height<<' '<<c.confidence_threshold<<' '<<c.nms_threshold<<' '<<c.person_class_id<<' '<<c.ground.enabled<<' '<<c.ground.cameras.size()<<' '<<c.zone.type<<' '<<c.zone.radius_m<<' '<<c.start_angle_deg<<' '<<c.stop_angle_deg<<' '<<c.min_frames<<'\n';for(auto&cam:c.ground.cameras)std::cout<<cam.id<<' '<<cam.mapper<<' '<<cam.matrix_path<<' '<<cam.calibration_width<<' '<<cam.calibration_height<<'\n';for(auto id:c.video_channel)std::cout<<id<<',';std::cout<<'\n';}}
