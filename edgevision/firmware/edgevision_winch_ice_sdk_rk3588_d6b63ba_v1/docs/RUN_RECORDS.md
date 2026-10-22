# 运行记录

records是本包demo在RK3588上的真实调用记录，不参与编译和算法运行，不必部署到产品程序目录。
完整demo的三组记录使用同一张704×576样例图，分别演示默认检测、阈值更新与日志、高阈值筛选。

## 完整demo的三组记录

| 目录 | 调用及结果 |
| --- | --- |
| default_run | 默认置信度门槛0.45、面积门槛0.5。返回7146个原始候选、1个卷扬对象，不报警。 |
| update_log_run | 注册日志回调，initialize后调用updateConfig将面积门槛改为0.1，再detect和release；结果报警。 |
| high_confidence_run | 将置信度门槛改为1.0。没有筛选后的卷扬对象，但原始候选仍是7146个，内容和顺序均不变。 |

默认卷扬框为(x=113, y=87, width=534, height=489)，confidence=0.951441228，ice_ratio=0.393097579（约39.31%）。原始候选中7128个评分为0，低于0.45的候选共7136个，均保留在snow_detections中。

## 完整demo每组文件

| 文件 | 内容 |
| --- | --- |
| command.txt | 当时执行的命令和工作目录；搬迁SDK后按README中的命令重新运行 |
| config.yaml | 读取时的初始配置；updateConfig后的实际配置见每帧JSON |
| sdk_version.json | 运行时的版本快照，保留原始内容 |
| input.png | 未画框的输入图片 |
| frame_000001.png | 卷扬框、模型置信度和HSV占比 |
| frame_000001.json | 本帧图像参数、实际配置、完整候选、卷扬结果和返回值 |
| result.json | initialize、updateConfig、detect、release等完整调用次序、输入输出及退出状态 |
| run.log | 完整控制台输出和日志 |

result.json中的frames字段引用每帧JSON和图片，因此这些文件需配套保留。
demo运行时还会生成首帧图片副本annotated.png；附带记录已去掉这份重复图片，保留内容相同的frame_000001.png。

样例没有真值标注，运行记录用于核对接口和输出，不作为现场精度或长期运行验收。

## 最小接入示例

`minimal_run/` 保存 `demo/minimal.cpp` 在RK3588上的独立编译和运行记录：

| 文件 | 内容 |
| --- | --- |
| command.txt | 编译、运行命令及退出码 |
| build.log | 使用g++链接配套库的完整编译输出 |
| run.log | 默认样例的控制台输出，含结果、候选数和对象字段 |
| sdk_version.json | 运行时的版本快照 |

最小示例只打印摘要，不生成画框图或完整候选JSON；这些输出见上面的完整demo记录。接口仍返回完整的snow_detections，示例未对它做阈值过滤或截断。

[快速运行](../README.md) · [API参数与调用时序](API.md)
