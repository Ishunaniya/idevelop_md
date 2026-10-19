# hytop Bash 历史逐行解释（已脱敏）

后续验证工作目录已改为 **`/home/hytop/l_evs`**（用户从 `/home/hytop/evs` 复制）。本文件是此前读取的历史记录，命令中的原目录按原样保留；它不是新目录的操作记录。

这份文档用于按行号查阅；先读 [主核验报告第 6 节](设备evs目录与history核验.md#6-history-记录的操作是什么) 的概览，再查看相关阶段即可，无需从头通读全部 1694 行。

编号来自本次读取的 `/home/hytop/.bash_history` 文件，表示文件行号。原文件没有逐条时间戳，也不记录退出码，因此以下解释说明操作意图，不能据此确认执行成功。第 961 行含 706 个 NUL 空字节，已显式标记。

快速查找：

- [1～239：ROS2、Qt 显示与触摸设备](#history-1)
- [240～329：进程、交换空间和构建脚本](#history-240)
- [330～603：硬件自检、压测与旧目录整理](#history-330)
- [604～789：crane_safety 编译、链接与调试](#history-604)
- [790～960：交换文件、告警系统与数据库调试](#history-790)
- [961～1133：异常历史行与 RKNN 运行环境](#history-961)
- [1134～1399：hook_config_rec 识别工程迭代](#history-1134)
- [1400～1607：hook_overwind 工程与测试](#history-1400)
- [1608～1668：EVS 摄像头、人员界面与录屏](#history-1608)
- [1669～1694：冰雪模型文件与冰雪界面](#history-1669)

<a id="history-1"></a>
## 1～239：ROS2、Qt 显示与触摸设备

主要是显示工程的构建、启动和触摸输入查询。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 1 | <code>./compile.sh 2</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 2 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 3 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 4 | <code>./compile.sh 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 5 | <code>./compile.sh 2</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 6 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 7 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 8 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 9 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 10 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 11 | <code>top</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 12 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 13 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 14 | <code>exit</code> | 退出当前 shell。 |
| 15 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 16 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 17 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 18 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 19 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 20 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 21 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 22 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 23 | <code>cd ros2_qt_display/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 24 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 25 | <code>file ros2_qt_display</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 26 | <code>./ros2_qt_display</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 27 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 28 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 29 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 30 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 31 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 32 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 33 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 34 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 35 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 36 | <code>cd /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 37 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 38 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 39 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 40 | <code>sudo mkswap /swapfile</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 41 | <code>sudo swapon /swapfile</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 42 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 43 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 44 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 45 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 46 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 47 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 48 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 49 | <code>rm -rf clean.sh </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 50 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 51 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 52 | <code>rm -rf cyclonedds.xml fastdds.xml </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 53 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 54 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 55 | <code>rm -rf run.sh run_sub.sh </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 56 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 57 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 58 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 59 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 60 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 61 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 62 | <code>cd perception_pkg/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 63 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 64 | <code>rm -rf test_subscriber/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 65 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 66 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 67 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 68 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 69 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 70 | <code>mkdir inference_pkg</code> | 建立工程/构建/测试目录。 |
| 71 | <code>mkdir test_pkg</code> | 建立工程/构建/测试目录。 |
| 72 | <code>rename test_pkg/ test_demo_pkg</code> | 尝试重命名，参数语义与工具版本有关。 |
| 73 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 74 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 75 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 76 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 77 | <code>cd /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 78 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 79 | <code>s</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 80 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 81 | <code>cd /home/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 82 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 83 | <code>cd hytop/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 84 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 85 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 86 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 87 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 88 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 89 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 90 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 91 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 92 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 93 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 94 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 95 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 96 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 97 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 98 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 99 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 100 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 101 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 102 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 103 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 104 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 105 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 106 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 107 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 108 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 109 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 110 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 111 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 112 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 113 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 114 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 115 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 116 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 117 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 118 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 119 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 120 | <code>cd cpu/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 121 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 122 | <code>exit</code> | 退出当前 shell。 |
| 123 | <code>cd src/ros2_qt_display/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 124 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 125 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 126 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 127 | <code>cd render/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 128 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 129 | <code>cp videofboitem.h cp videofboitem.cpp ../video/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 130 | <code>cp -r videofboitem.h cp videofboitem.cpp ../video/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 131 | <code>mv videofboitem.h ../video/</code> | 移动/重命名文件或目录。 |
| 132 | <code>mv videofboitem.cpp ../video/</code> | 移动/重命名文件或目录。 |
| 133 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 134 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 135 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 136 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 137 | <code>rm -rf render/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 138 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 139 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 140 | <code>cd video/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 141 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 142 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 143 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 144 | <code>rm -rf videochannel.cpp videochannel.h </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 145 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 146 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 147 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 148 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 149 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 150 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 151 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 152 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 153 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 154 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 155 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 156 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 157 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 158 | <code>./compile.sh 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 159 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 160 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 161 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 162 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 163 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 164 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 165 | <code>./compile.sh</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 166 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 167 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 168 | <code>rm -rf build/ install/ log/ logs</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 169 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 170 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 171 | <code>ls -a</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 172 | <code>rm -rf .cache/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 173 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 174 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 175 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 176 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 177 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 178 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 179 | <code>cd X_Project/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 180 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 181 | <code>cd gst_crane_ws/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 182 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 183 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 184 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 185 | <code>cd ros2_qt_display/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 186 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 187 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 188 | <code>./ros2_qt_display</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 189 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 190 | <code>exit</code> | 退出当前 shell。 |
| 191 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 192 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 193 | <code>cat /proc/bus/input/devices</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 194 | <code>cat /sys/class/input/input*/threshold</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 195 | <code>cd /sys/class/input/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 196 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 197 | <code>cd input1</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 198 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 199 | <code>cat /proc/bus/input/devices </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 200 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 201 | <code>dmesg &#124; grep -i goodix</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 202 | <code>evtest</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 203 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 204 | <code>cd X_Project/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 205 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 206 | <code>cd gst_crane_ws/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 207 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 208 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 209 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 210 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 211 | <code>cd ros2_qt_display/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 212 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 213 | <code>./ros2_qt_display</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 214 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 215 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 216 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 217 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 218 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 219 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 220 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 221 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 222 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 223 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 224 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 225 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 226 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 227 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 228 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 229 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 230 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 231 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 232 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 233 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 234 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 235 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 236 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 237 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 238 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 239 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |

<a id="history-240"></a>
## 240～329：进程、交换空间和构建脚本

包含进程处理、交换空间、fstab 和重启相关操作；历史未记录执行结果。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 240 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 241 | <code>cd log/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 242 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 243 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 244 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 245 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 246 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 247 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 248 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 249 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 250 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 251 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 252 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 253 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 254 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 255 | <code>./cl</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 256 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 257 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 258 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 259 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 260 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 261 | <code>sudo mkswap /swapfile</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 262 | <code>sudo swapon /swapfile</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 263 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 264 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 265 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 266 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 267 | <code>ps -au </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 268 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 269 | <code>top</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 270 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 271 | <code>ps aux &#124; grep ros2-qt_display</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 272 | <code>kill -9 13088</code> | 终止指定进程；kill -9 是强制终止，本轮未执行。 |
| 273 | <code>kill -9 9472</code> | 终止指定进程；kill -9 是强制终止，本轮未执行。 |
| 274 | <code>kill -9 1848</code> | 终止指定进程；kill -9 是强制终止，本轮未执行。 |
| 275 | <code>ps aux &#124; grep ros2_qt_display</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 276 | <code>kill -9 12763</code> | 终止指定进程；kill -9 是强制终止，本轮未执行。 |
| 277 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 278 | <code>exit</code> | 退出当前 shell。 |
| 279 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 280 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 281 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 282 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 283 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 284 | <code>./compile.sh 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 285 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 286 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 287 | <code>source install/setup.bash </code> | 把 ROS2 安装环境载入当前 shell。 |
| 288 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 289 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 290 | <code>ros2 run ros2_qt_display ros2_qt_display </code> | 启动/查询 ROS2 图形显示节点。 |
| 291 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 292 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 293 | <code>cd /etc</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 294 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 295 | <code>sudo cp /etc/fstab /etc/fstab.bak</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 296 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 297 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 298 | <code>sudo cat fstab</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 299 | <code>echo '/swapfile none swap sw 0 0' &#124; sudo tee -a /etc/fstab</code> | 输出文本；此历史中用于把 swap 配置追加到 /etc/fstab。 |
| 300 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 301 | <code>cat /etc/fstab &#124; grep swapfile</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 302 | <code>sudo reboot</code> | 尝试设置交换空间、持久化挂载或重启，属于系统配置操作；本轮未执行。 |
| 303 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 304 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 305 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 306 | <code>cd ~</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 307 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 308 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 309 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 310 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 311 | <code>cat .sudo_as_admin_successful </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 312 | <code>rm -rf .sudo_as_admin_successful </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 313 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 314 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 315 | <code>./cpu/</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 316 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 317 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 318 | <code>./compile.sh </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 319 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 320 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 321 | <code>chmod u+x compile_c.sh </code> | 修改可执行权限；末段用于 UI 和相机程序。 |
| 322 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 323 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 324 | <code>./compile_c.sh -h</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 325 | <code>./compile_c.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 326 | <code>./compile_c.sh 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 327 | <code>./compile_c.sh -c 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 328 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 329 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |

<a id="history-330"></a>
## 330～603：硬件自检、压测与旧目录整理

包含 CPU、内存、磁盘查询，stress 压测和 ROS2 相关调整。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 330 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 331 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 332 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 333 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 334 | <code>rm -rf compile.sh </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 335 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 336 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 337 | <code>mv compile_c.sh compile.sh</code> | 移动/重命名文件或目录。 |
| 338 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 339 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 340 | <code>cd sd_checker</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 341 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 342 | <code>cd sd_checker/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 343 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 344 | <code>cd build</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 345 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 346 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 347 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 348 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 349 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 350 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 351 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 352 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 353 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 354 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 355 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 356 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 357 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 358 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 359 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 360 | <code>free &#124; awk 'NR==2{printf "总内存: %.1fG, 可用内存: %.1fG, 使用率: %.1f%%\n", $2/1024/1024, $7/1024/1024, (1-$7/$2)*100}'</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 361 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 362 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 363 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 364 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 365 | <code>htop</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 366 | <code>cpustat</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 367 | <code>cat /proc/stat</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 368 | <code>top</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 369 | <code>cat /sys/class/thermal/thermal_zone0/temp 2&gt;/dev/null</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 370 | <code>sudo cpufreq-set -g userspace</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 371 | <code>sudo cpufreq-set -f 1800000</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 372 | <code>cpufreq-info</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 373 | <code>watch -n 1 "cat /proc/cpuinfo &#124; grep 'cpu MHz'"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 374 | <code>stress --cpu 8 --timeout 60s</code> | CPU 压力测试或版本查询；--cpu 8 --timeout 60s 表示 8 路 CPU 压力运行 60 秒。 |
| 375 | <code>stress --version</code> | CPU 压力测试或版本查询；--cpu 8 --timeout 60s 表示 8 路 CPU 压力运行 60 秒。 |
| 376 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 377 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 378 | <code>stress --version</code> | CPU 压力测试或版本查询；--cpu 8 --timeout 60s 表示 8 路 CPU 压力运行 60 秒。 |
| 379 | <code>sudo apt update</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 380 | <code>sudo apt install stress -y</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 381 | <code>version stress</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 382 | <code>version -stress</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 383 | <code>version --stress</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 384 | <code>stress --version</code> | CPU 压力测试或版本查询；--cpu 8 --timeout 60s 表示 8 路 CPU 压力运行 60 秒。 |
| 385 | <code>stress --cpu 8 --timeout 60s</code> | CPU 压力测试或版本查询；--cpu 8 --timeout 60s 表示 8 路 CPU 压力运行 60 秒。 |
| 386 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 387 | <code>cd sd_checker/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 388 | <code>LS</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 389 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 390 | <code>cd sd_checker/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 391 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 392 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 393 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 394 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 395 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 396 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 397 | <code>./hw_checker_demo </code> | 运行硬件自检示例。 |
| 398 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 399 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 400 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 401 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 402 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 403 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 404 | <code>cd  /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 405 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 406 | <code>cd /home/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 407 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 408 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 409 | <code>cd hytop/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 410 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 411 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 412 | <code>cd X_Project/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 413 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 414 | <code>rm -rf tcp_ip/ wrmd_syzn UnixSocket/ ros2_ws/ ros2_qt_display</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 415 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 416 | <code>rm -rf crane_ws_fps2/ crane_ws_dma/ rtc crane_ws_option/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 417 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 418 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 419 | <code>cat dmc_gpu_npu.sh </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 420 | <code>rm -rf dmc_gpu_npu.sh </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 421 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 422 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 423 | <code>rm -rf opencv_example</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 424 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 425 | <code>rm -rf crane_ws_02_26</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 426 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 427 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 428 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 429 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 430 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 431 | <code>cd /home/hs/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 432 | <code>cd /home/hytop/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 433 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 434 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 435 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 436 | <code>rm -rf .vscode-server/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 437 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 438 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 439 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 440 | <code>cd /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 441 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 442 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 443 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 444 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 445 | <code>free -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 446 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 447 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 448 | <code>cd /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 449 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 450 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 451 | <code>cd /home/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 452 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 453 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 454 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 455 | <code>cd h</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 456 | <code>cd hytop/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 457 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 458 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 459 | <code>du -sh .[!.]* * 2&gt;/dev/null &#124; sort -hr</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 460 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 461 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 462 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 463 | <code>cat test.cpp </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 464 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 465 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 466 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 467 | <code>rm -rf test/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 468 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 469 | <code>mkdir self_check</code> | 建立工程/构建/测试目录。 |
| 470 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 471 | <code>cd self_check/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 472 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 473 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 474 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 475 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 476 | <code>mv cpu/ self_check/</code> | 移动/重命名文件或目录。 |
| 477 | <code>mv memory_checker/ self_check/</code> | 移动/重命名文件或目录。 |
| 478 | <code>mv sd_checker/ self_check/</code> | 移动/重命名文件或目录。 |
| 479 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 480 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 481 | <code>cd X_Project/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 482 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 483 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 484 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 485 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 486 | <code>rm -rf wrmd_syzn/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 487 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 488 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 489 | <code>./compile.sh -h</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 490 | <code>./compile.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 491 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 492 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 493 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 494 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 495 | <code>./compile.sh -h</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 496 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 497 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 498 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 499 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 500 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 501 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 502 | <code>./compile.sh -r 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 503 | <code>./compile.sh -r 2</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 504 | <code>ros2 node list</code> | 启动/查询 ROS2 图形显示节点。 |
| 505 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 506 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 507 | <code>./compile.sh -r 2</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 508 | <code>./compile.sh -r 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 509 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 510 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 511 | <code>./compile.sh -r 1</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 512 | <code>./compile.sh -r 2</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 513 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 514 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 515 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 516 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 517 | <code>./compile.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 518 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 519 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 520 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 521 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 522 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 523 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 524 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 525 | <code>./compile.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 526 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 527 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 528 | <code>./compile.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 529 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 530 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 531 | <code>./compile.sh -c</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 532 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 533 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 534 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 535 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 536 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 537 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 538 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 539 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 540 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 541 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 542 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 543 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 544 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 545 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 546 | <code>./compile.sh -c 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 547 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 548 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 549 | <code>./compile.sh -r 3</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 550 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 551 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 552 | <code>clera</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 553 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 554 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 555 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 556 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 557 | <code>cd sany_app/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 558 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 559 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 560 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 561 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 562 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 563 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 564 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 565 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 566 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 567 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 568 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 569 | <code>cd gst_crane_ws/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 570 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 571 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 572 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 573 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 574 | <code>cd perception_pkg/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 575 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 576 | <code>cp package.xml ../test_pkg/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 577 | <code>cp CMakeLists.txt ../test_pkg/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 578 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 579 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 580 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 581 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 582 | <code>cd test_pkg/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 583 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 584 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 585 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 586 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 587 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 588 | <code>cd test_pkg/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 589 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 590 | <code>cd node_publisher/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 591 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 592 | <code>mv CMakeLists.txt ../</code> | 移动/重命名文件或目录。 |
| 593 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 594 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 595 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 596 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 597 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 598 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 599 | <code>colcon list</code> | 查询 ROS2 工程包列表。 |
| 600 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 601 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 602 | <code>./compile.sh 4</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 603 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |

<a id="history-604"></a>
## 604～789：crane_safety 编译、链接与调试

主要围绕算法库、测试程序和 RKNN、RGA、OpenCV 依赖排障。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 604 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 605 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 606 | <code>cd crane_safety/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 607 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 608 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 609 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 610 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 611 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 612 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 613 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 614 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 615 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 616 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 617 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 618 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 619 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 620 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 621 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 622 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 623 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 624 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 625 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 626 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 627 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 628 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 629 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 630 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 631 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 632 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 633 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 634 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 635 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 636 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 637 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 638 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 639 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 640 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 641 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 642 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 643 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 644 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 645 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 646 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 647 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 648 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 649 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 650 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 651 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 652 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 653 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 654 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 655 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 656 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 657 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 658 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 659 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 660 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 661 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 662 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 663 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 664 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 665 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 666 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 667 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 668 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 669 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 670 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 671 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 672 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 673 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 674 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 675 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 676 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 677 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 678 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 679 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 680 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 681 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 682 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 683 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 684 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 685 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 686 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 687 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 688 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 689 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 690 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 691 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 692 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 693 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 694 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 695 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 696 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 697 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 698 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 699 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 700 | <code>cmake</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 701 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 702 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 703 | <code>cp ./libcrane_safety.so ../test/libcrane_safety.so</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 704 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 705 | <code>cp include/crane/hook_config_rec.hpp ./test/hook_config_rec.hpp</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 706 | <code>cp include/crane/common/types.hpp ./test/types.hpp</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 707 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 708 | <code>ped</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 709 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 710 | <code>g++ test.cpp -0 test</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 711 | <code>g++ test.cpp -o test</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 712 | <code>cp ../include/crane/common/error_code.hpp ./error_code.hpp</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 713 | <code>g++ test.cpp -o test</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 714 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 715 | <code>gdb ./test</code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 716 | <code>g++ test.cpp -o test</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 717 | <code>g++ -std=c++17 test.cpp -o test -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 718 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 719 | <code>g++ -std=c++17 test.cpp -o test -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 720 | <code>cls</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 721 | <code>clc</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 722 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 723 | <code>g++ -std=c++17 test.cpp -o test -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 724 | <code>gdb ./test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 725 | <code>g++ -std=c++17 test.cpp -o test -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 726 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/ -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 727 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 728 | <code>mv ./hook_config_rec.hpp ./lib/hook_config_rec.hpp</code> | 移动/重命名文件或目录。 |
| 729 | <code>mv ./libcrane_safety.so ./lib/libcrane_safety.so</code> | 移动/重命名文件或目录。 |
| 730 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 731 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 732 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 733 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 734 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -lrknn &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 735 | <code>ldd ./lib/libcrane_safety.so </code> | 检查目标动态库依赖，用于定位 RKNN/RGA/OpenCV 链接问题。 |
| 736 | <code>find_library(RKNN_LIBRARY NAMES rknn rknnrt)</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 737 | <code>sudo find / -name "librknn*.so*" 2&gt;/dev/null</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 738 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 739 | <code>cd ../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 740 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 741 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 742 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 743 | <code>sudo find / -name "librga.so*" 2&gt;/dev/null</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 744 | <code>sudo find / -name "rga.h*" 2&gt;/dev/null</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 745 | <code>rm -r **</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 746 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 747 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 748 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 749 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 750 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 751 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 752 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 753 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 754 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 755 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 756 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 757 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 758 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 759 | <code>cmake</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 760 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 761 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 762 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 763 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 764 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 765 | <code>cp libcrane_safety.so ../test/lib/libcrane_safety.so</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 766 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 767 | <code>cd ../test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 768 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 769 | <code>sudo find / -name "librknn*.so*" 2&gt;/dev/null</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 770 | <code>sudo find / -name "*rga.so*" 2&gt;/dev/null</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 771 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt -L/usr/lib/aarch64-linux-gnu/ -lrga&#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 772 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt -L/usr/lib/aarch64-linux-gnu/ -lrga &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 773 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 774 | <code>gdb ./test</code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 775 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt -L/usr/lib/aarch64-linux-gnu/ -lrga &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 776 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 777 | <code>g++ -std=c++17 test.cpp -o test -L/home/hytop/crane_vision/crane_safety/test/lib/ -I/usr/include/opencv4 -I/home/hytop/crane_vision/crane_safety/test/ -I/home/hytop/crane_vision/crane_safety/test/lib/ -lcrane_safety -L/usr/lib/ -lrknnrt -L/usr/lib/aarch64-linux-gnu/ -lrga &#96;pkg-config opencv4 --cflags --libs&#96;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 778 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 779 | <code>export LD_LIBRARY_PATH=/home/hytop/crane_vision/crane_safety/test/lib:$LD_LIBRARY_PATH</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 780 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 781 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 782 | <code>./test</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 783 | <code>export LD_LIBRARY_PATH=/home/hytop/crane_visks</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 784 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 785 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 786 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 787 | <code>/</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 788 | <code>cd /</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 789 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |

<a id="history-790"></a>
## 790～960：交换文件、告警系统与数据库调试

包含删除尝试、库调试及关机命令；不能由命令本身判断成功。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 790 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 791 | <code>rm -rf swapfile </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 792 | <code>su</code> | 切换用户或修改密码；后续目标用户的历史不在本次 hytop .bash_history 中。 |
| 793 | <code>cd test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 794 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 795 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 796 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 797 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 798 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 799 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 800 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 801 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 802 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 803 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 804 | <code>cd ~/.vscode-server/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 805 | <code>ls -lh</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 806 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 807 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 808 | <code>cd X_Project/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 809 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 810 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 811 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 812 | <code>./test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 813 | <code>ifconfig</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 814 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 815 | <code>ccd </code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 816 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 817 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 818 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 819 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 820 | <code>ld</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 821 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 822 | <code>rm -rf build </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 823 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 824 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 825 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 826 | <code>rm -rf alarms.db </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 827 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 828 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 829 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 830 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 831 | <code>cd build</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 832 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 833 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 834 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 835 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 836 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 837 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 838 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 839 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 840 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 841 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 842 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 843 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 844 | <code>cd build</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 845 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 846 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 847 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 848 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 849 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 850 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 851 | <code>vim AlarmManager.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 852 | <code>vim main.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 853 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 854 | <code>cd alarm_system</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 855 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 856 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 857 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 858 | <code>cd build</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 859 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 860 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 861 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 862 | <code>vim AlarmManager.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 863 | <code>ifconfig</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 864 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 865 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 866 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 867 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 868 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 869 | <code>rm -rf *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 870 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 871 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 872 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 873 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 874 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 875 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 876 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 877 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 878 | <code>rm -rf *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 879 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 880 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 881 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 882 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 883 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 884 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 885 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 886 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 887 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 888 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 889 | <code>cd </code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 890 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 891 | <code>cd ubuntu</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 892 | <code>cd /home/ubuntu/qml/alarm_system/data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 893 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 894 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 895 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 896 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 897 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 898 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 899 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 900 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 901 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 902 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 903 | <code>cd </code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 904 | <code>cd alarm_system/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 905 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 906 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 907 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 908 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 909 | <code>cd build</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 910 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 911 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 912 | <code>./alarm_system</code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 913 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 914 | <code>cd data</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 915 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 916 | <code>pkg -connnfig --modversion opencv4</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 917 | <code>pkg-connnfig --modversion opencv4</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 918 | <code>striings /usr/lib/aarch64-linux-gnu/libopencv_core.so &#124; grep versionn</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 919 | <code>strings /usr/lib/aarch64-linux-gnu/libopencv_core.so &#124; grep versionn</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 920 | <code>strings /usr/lib/aarch64-linux-gnu/libopencv_core.so &#124; grep version</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 921 | <code>keybse-</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 922 | <code>cd test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 923 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 924 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 925 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 926 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 927 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 928 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 929 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 930 | <code>cp libhook_config_rec.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 931 | <code>cd ../test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 932 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 933 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 934 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 935 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 936 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 937 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 938 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 939 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 940 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 941 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 942 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 943 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 944 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 945 | <code>cd crane_safety</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 946 | <code>cd test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 947 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 948 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 949 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 950 | <code>cmmake ..</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 951 | <code>cmake..</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 952 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 953 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 954 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 955 | <code>pdb ./lib_test </code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 956 | <code>gdb ./lib_test </code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 957 | <code>power off</code> | 关机/关闭的尝试，含非标准或不完整写法；无法从记录确认结果。 |
| 958 | <code>shutoff</code> | 关机/关闭的尝试，含非标准或不完整写法；无法从记录确认结果。 |
| 959 | <code>shutdown</code> | 关机/关闭的尝试，含非标准或不完整写法；无法从记录确认结果。 |
| 960 | <code>poweroff</code> | 关机/关闭的尝试，含非标准或不完整写法；无法从记录确认结果。 |

<a id="history-961"></a>
## 961～1133：异常历史行与 RKNN 运行环境

起始行含空字节，后续主要是运行库、头文件、系统依赖和启动脚本处理。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 961 | <code>[NUL×706]cd test/build/</code> | 此行含空字节，历史记录不完整；不能按正常完整命令解释。 |
| 962 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 963 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 964 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 965 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 966 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 967 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 968 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 969 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 970 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 971 | <code>make </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 972 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 973 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 974 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 975 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 976 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 977 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 978 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 979 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 980 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 981 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 982 | <code>cp libhook_config_rec.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 983 | <code>cd ../test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 984 | <code>./lib_test </code> | 尝试运行本地测试/演示程序；功能和结果取决于当时文件与参数。 |
| 985 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 986 | <code>cd release/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 987 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 988 | <code>tar -czf libHookConfigRec-RK3588.tar.gz ./libHookConfigRec-RK3588/</code> | 打包源码/部署目录；evs_onboard.tar.gz 的存在与位置需另查。 |
| 989 | <code>find /usr/ -name "librknn.so"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 990 | <code>cd /usr/lib/aarch64-linux-gnu/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 991 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 992 | <code>find ./ -name "*rknn*"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 993 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 994 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 995 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 996 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 997 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 998 | <code>cd crane_safety</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 999 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1000 | <code>cd 3rdparty/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1001 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1002 | <code>cd rknn_api/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1003 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1004 | <code>cd lib/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1005 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1006 | <code>string ./librknnrt.so &#124; grep -i "librknnrt version"</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1007 | <code>strings ./librknnrt.so &#124; grep -i "librknnrt version"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1008 | <code>cat /sys/kernel/debug/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1009 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1010 | <code>sudo find / -name "librknnrt.so"</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 1011 | <code>strings /usr/lib/librknnrt.so &#124; grep -i version</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1012 | <code>strings /usr/lib/librknnrt.so &#124; grep -i librknnrt</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1013 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1014 | <code>cd ../../../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1015 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1016 | <code>ca crane_safety</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1017 | <code>cd crane_safety</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1018 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1019 | <code>cat CMakeLists.txt </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1020 | <code>cd </code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1021 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1022 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1023 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1024 | <code>cd crane_safety</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1025 | <code>cat CMakeLists.txt </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1026 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1027 | <code>cd /usr/include/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1028 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1029 | <code>cd rockchip/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1030 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1031 | <code>cd /usr/lib/aarch64-linux-gnu/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1032 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1033 | <code>strings ./librknnrt.so &#124; grep -i librknnrt</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1034 | <code>find /usr -name librknnrt.so</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1035 | <code>strings /usr/lib/librknnrt.so &#124; grep -i librknnrt</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1036 | <code>cd /usr/lib</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1037 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1038 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1039 | <code>cd include/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1040 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1041 | <code>sudo find / -name "rknn_api.h"</code> | 以管理员权限执行命令的尝试，history 不证明认证或命令成功。 |
| 1042 | <code>sh /usr/bin/restart_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1043 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1044 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1045 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1046 | <code>cd wrmd_syzn/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1047 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1048 | <code>cd 3rdparty/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1049 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1050 | <code>strings ./rknn_api/lib/librknnrt.so &#124; grep -i librknnrt</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1051 | <code>cat ./rknn_api/include/rknn_api.h </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1052 | <code>cat -h 20 ./rknn_api/include/rknn_api.h </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1053 | <code>cat -n 20 ./rknn_api/include/rknn_api.h </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1054 | <code>vim ./rknn_api/include/rknn_api.h </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1055 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1056 | <code>cd opencv/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1057 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1058 | <code>ls -l ./lib/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1059 | <code>cd ../../../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1060 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1061 | <code>mkdir linux</code> | 建立工程/构建/测试目录。 |
| 1062 | <code>cd linux/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1063 | <code>mkdir rknn_api</code> | 建立工程/构建/测试目录。 |
| 1064 | <code>mkdir rknn_server</code> | 建立工程/构建/测试目录。 |
| 1065 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1066 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1067 | <code>cd crane_vision/linux/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1068 | <code>ls -l /usr/lib/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1069 | <code>ls -l /usr/bin/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1070 | <code>sudo install -m 644 ./rknn_api/librknnrt.so /usr/lib/</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 1071 | <code>sudo install -m 755 ./rknn_server/ /usr/bin/</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 1072 | <code>strings /usr/lib/librknnrt.so &#124; grep -i librknnrt</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1073 | <code>sh /usr/bin/start_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1074 | <code>sh /usr/bin/restart_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1075 | <code>cd /usr/bin</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1076 | <code>mv ./rknn_server ./rknn_server_back</code> | 移动/重命名文件或目录。 |
| 1077 | <code>mv ./start_rknn.sh ./start_rknn.sh.back</code> | 移动/重命名文件或目录。 |
| 1078 | <code>mv ./restart_rknn.sh ./restart_rknn.sh.back</code> | 移动/重命名文件或目录。 |
| 1079 | <code>cd</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1080 | <code>cd crane_vision/linux/rknn_server/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1081 | <code>cp restart_rknn.sh /usr/bin/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1082 | <code>cp rknn_server /usr/bin/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1083 | <code>cp start_rknn.sh /usr/bin/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1084 | <code>sh /usr/bin/start_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1085 | <code>cd /usr/bin</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1086 | <code>ls -l start_rknn.sh</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1087 | <code>sudo chmod 755 ./start_rknn.sh</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 1088 | <code>sh /usr/bin/start_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1089 | <code>ls -l start_rknn.sh</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1090 | <code>sudo chmod 755 ./rknn_server</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 1091 | <code>sh /usr/bin/start_rknn.sh </code> | 运行 RKNN 服务启动/重启脚本；本轮未运行。 |
| 1092 | <code>sudo chmod 755 ./restart_rknn.sh</code> | 尝试安装依赖/系统运行库、调整权限或 CPU 频率；需另查结果。 |
| 1093 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1094 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1095 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1096 | <code>help zip</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1097 | <code>man zip</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1098 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1099 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1100 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1101 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1102 | <code>unzip -p hook_config_rec.zip </code> | 查看/解包归档。 |
| 1103 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1104 | <code>cd hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1105 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1106 | <code>ls -l /usr/lib</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1107 | <code>find /usr/lib/ -name "librga.so"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1108 | <code>ls -l /usr/lib/aarch64-linux-gnu/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1109 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1110 | <code>cmake --help-module Find&lt;Name&gt;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1111 | <code>ls -l /usr/include/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1112 | <code>ls -l /usr/include/rga/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1113 | <code>cmake --help-module Find&lt;rknn&gt;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1114 | <code>dpkg -l &#124; grep librga.*-dev</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1115 | <code>dpkg -L librga-dev &#124; grep -iE "Config\.cmake&#124;rga.*\.cmake"</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1116 | <code>pkg-config --exits rga</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1117 | <code>pkg-config --exists rga</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1118 | <code>pkg-config --cflags --libs rga</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1119 | <code>pkg-config --list-all &#124; grep -i rga</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1120 | <code>pkg-config --cflags --libs librga</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1121 | <code>pkg-config --exists librga &amp;&amp; echo "yes" &#124;&#124; echo "no"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1122 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1123 | <code>cmake --help-module Find&lt;rga&gt;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1124 | <code>pkg-config --exists librknn &amp;&amp; echo "yes" &#124;&#124; echo "no"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1125 | <code>cmake --help-module Find&lt;rga&gt;</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1126 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1127 | <code>pkg-config --exists OpenCv &amp;&amp; echo "yes" &#124;&#124; echo "no"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1128 | <code>pkg-config --exists opencv &amp;&amp; echo "yes" &#124;&#124; echo "no"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1129 | <code>find /usr/ -name "OpenCV*"</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1130 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1131 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1132 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1133 | <code>cmake ../</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |

<a id="history-1134"></a>
## 1134～1399：hook_config_rec 识别工程迭代

包含模型服务、前后处理、类型修改，以及反复构建、复制测试库。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 1134 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1135 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1136 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1137 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1138 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1139 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1140 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1141 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1142 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1143 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1144 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1145 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1146 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1147 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1148 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1149 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1150 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1151 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1152 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1153 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1154 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1155 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1156 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1157 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1158 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1159 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1160 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1161 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1162 | <code>vim ../src/hook_config_rec.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1163 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1164 | <code>vim ../src/hook_config_rec.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1165 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1166 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1167 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1168 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1169 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1170 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1171 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1172 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1173 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1174 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1175 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1176 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1177 | <code>vim ../src/hook_config_rec.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1178 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1179 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1180 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1181 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1182 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1183 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1184 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1185 | <code>cd crane_vision/hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1186 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1187 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1188 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1189 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1190 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1191 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1192 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1193 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1194 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1195 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1196 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1197 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1198 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1199 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1200 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1201 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1202 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1203 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1204 | <code></code> | 空行。 |
| 1205 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1206 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1207 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1208 | <code>vim ../include/hook_config_rec.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1209 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1210 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1211 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1212 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1213 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1214 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1215 | <code>vim ../src/impl/hook_config_detector_impl.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1216 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1217 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1218 | <code>vim ../src/common/types.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1219 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1220 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1221 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1222 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1223 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1224 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1225 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1226 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1227 | <code>vim ../src/impl/hook_config_detector_impl.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1228 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1229 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1230 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1231 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1232 | <code>vim ../src/impl/hook_config_detector_impl.cpp</code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1233 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1234 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1235 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1236 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1237 | <code>vim ../src/impl/hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1238 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1239 | <code>vim ../src/rknn_model/model_service.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1240 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1241 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1242 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1243 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1244 | <code>vim ../src/process/yolov5_postprocess.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1245 | <code>vim ../src/impl/hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1246 | <code>vim ../src/rknn_model/model_service.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1247 | <code>vim ../src/process/yolov5_postprocess.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1248 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1249 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1250 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1251 | <code>vim ../src/process/yolov5_postprocess.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1252 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1253 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1254 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1255 | <code>vim ../src/process/postprocess.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1256 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1257 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1258 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1259 | <code>vim ../src/common/types.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1260 | <code>vim ../src/process/postprocess.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1261 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1262 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1263 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1264 | <code>vim ../src/process/preprocess.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1265 | <code>vim ../src/common/types.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1266 | <code>vim ../src/process/preprocess.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1267 | <code>vim ../src/common/types.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1268 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1269 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1270 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1271 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1272 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1273 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1274 | <code>cd ~</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1275 | <code>cd crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1276 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1277 | <code>cd crane_safety</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1278 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1279 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1280 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1281 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1282 | <code>cd test2</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1283 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1284 | <code>rm -rf build/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1285 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1286 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1287 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1288 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1289 | <code>cmake ../</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1290 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1291 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1292 | <code>cd  CMakeFiles/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1293 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1294 | <code>cd CMakeTmp/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1295 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1296 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1297 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1298 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1299 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1300 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1301 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1302 | <code>cd include/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1303 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1304 | <code>cat hook_config_rec.hpp </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1305 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1306 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1307 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1308 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1309 | <code>cd lib/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1310 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1311 | <code>file libhook_config_rec.so </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1312 | <code>ldd libhook_config_rec.so </code> | 检查目标动态库依赖，用于定位 RKNN/RGA/OpenCV 链接问题。 |
| 1313 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1314 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1315 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1316 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1317 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1318 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1319 | <code>cat hook_config_test.cpp </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1320 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1321 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1322 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1323 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1324 | <code>cd lib/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1325 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1326 | <code>file libhook_config_rec.so </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1327 | <code>ldd libhook_config_rec.so </code> | 检查目标动态库依赖，用于定位 RKNN/RGA/OpenCV 链接问题。 |
| 1328 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1329 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1330 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1331 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1332 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1333 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1334 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1335 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1336 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1337 | <code>cd test2</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1338 | <code>ks</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1339 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1340 | <code>rm -rf build/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1341 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1342 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1343 | <code>cmake ../</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1344 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1345 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1346 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1347 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1348 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1349 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1350 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1351 | <code>cd ~/crane_vision/hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1352 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1353 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1354 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1355 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1356 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1357 | <code>cd impl/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1358 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1359 | <code>vim hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1360 | <code>vim hook_config_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1361 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1362 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1363 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1364 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1365 | <code>rm -rf build/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1366 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1367 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1368 | <code>cmake ../</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1369 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1370 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1371 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1372 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1373 | <code>cp libhook_config_rec.so ~/crane_vision/crane_safety/test/test2/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1374 | <code>cd ~/crane_vision/crane_safety/test/test2</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1375 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1376 | <code>cp libhook_config_rec.so ./lib</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1377 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1378 | <code>cd lib/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1379 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1380 | <code>file libhook_config_rec.so </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1381 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1382 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1383 | <code>rm -rf build/</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1384 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1385 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1386 | <code>cmake ../</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1387 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1388 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1389 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1390 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1391 | <code>cd ~/crane_vision/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1392 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1393 | <code>cd hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1394 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1395 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1396 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1397 | <code>cd impl/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1398 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1399 | <code>vim hook_config_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |

<a id="history-1400"></a>
## 1400～1607：hook_overwind 工程与测试

包含工程目录整理、编译、库复制和测试程序执行记录。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 1400 | <code>cd crane_vision/hook_overwind/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1401 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1402 | <code>clear</code> | 清屏意图；非 Bash 内建的拼写是否有效取决于环境。 |
| 1403 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1404 | <code>mkdir test</code> | 建立工程/构建/测试目录。 |
| 1405 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1406 | <code>mv hoot_overwind_test.cpp ./test/</code> | 移动/重命名文件或目录。 |
| 1407 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1408 | <code>touh CMakeLists.txt</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1409 | <code>touch CMakeLists.txt</code> | 创建文件或更新时间。 |
| 1410 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1411 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1412 | <code>make ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1413 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1414 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1415 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1416 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1417 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1418 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1419 | <code>vim ../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1420 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1421 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1422 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1423 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1424 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1425 | <code>mkdir include</code> | 建立工程/构建/测试目录。 |
| 1426 | <code>mkdir lib</code> | 建立工程/构建/测试目录。 |
| 1427 | <code>mkdir build</code> | 建立工程/构建/测试目录。 |
| 1428 | <code>mkdir images</code> | 建立工程/构建/测试目录。 |
| 1429 | <code>cp ../build/libhook_overwind.so lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1430 | <code>ls -l lib/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1431 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1432 | <code>cd ../crane_safety/test/test2/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1433 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1434 | <code>cp CMakeLists.txt ../../../hook_overwind/test/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1435 | <code>cd ../../../hook_overwind/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1436 | <code>cd test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1437 | <code>vim CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1438 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1439 | <code>mkdir src</code> | 建立工程/构建/测试目录。 |
| 1440 | <code>mv hoot_overwind_test.cpp ./src/</code> | 移动/重命名文件或目录。 |
| 1441 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1442 | <code>mv hoot_overwind_test.cpp hook_overwind_test.cpp</code> | 移动/重命名文件或目录。 |
| 1443 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1444 | <code>cd ../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1445 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1446 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1447 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1448 | <code>ls -l ../include/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1449 | <code>cp ../../include/hook_overwind.hpp ../include/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1450 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1451 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1452 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1453 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1454 | <code>cp ../../../../crane_vision/crane_safety/test/test_images/main_aux_hook.jpg ../images/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1455 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1456 | <code>cd ../images/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1457 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1458 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1459 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1460 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1461 | <code>cd ../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1462 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1463 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1464 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1465 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1466 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1467 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1468 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1469 | <code>vim ../CMakeLists.txt </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1470 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1471 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1472 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1473 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1474 | <code>cd ../src/impl/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1475 | <code>vim hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1476 | <code>vim hook_overwind_detector_impl.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1477 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1478 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1479 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1480 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1481 | <code>cp libhook_overwind.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1482 | <code>cd ../test/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1483 | <code>cd build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1484 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1485 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1486 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1487 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1488 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1489 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1490 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1491 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1492 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1493 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1494 | <code>cd ../../src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1495 | <code>vim impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1496 | <code>cd ../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1497 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1498 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1499 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1500 | <code>cp libhook_overwind.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1501 | <code>cd ../test/src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1502 | <code>cim hook_overwind_test.cpp</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1503 | <code>vim hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1504 | <code>cd ../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1505 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1506 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1507 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1508 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1509 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1510 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1511 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1512 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1513 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1514 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1515 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1516 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1517 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1518 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1519 | <code>cd ../../src/impl/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1520 | <code>vim hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1521 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1522 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1523 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1524 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1525 | <code>vim ../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1526 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1527 | <code>cmake </code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1528 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1529 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1530 | <code>vim ../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1531 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1532 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1533 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1534 | <code>cp libhook_overwind.so ../test/lib/libhook_overwind.so </code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1535 | <code>cd ../test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1536 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1537 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1538 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1539 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1540 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1541 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1542 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1543 | <code>vim ../../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1544 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1545 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1546 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1547 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1548 | <code>vim ../../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1549 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1550 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1551 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1552 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1553 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1554 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1555 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1556 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1557 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1558 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1559 | <code>vim ../../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1560 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1561 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1562 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1563 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1564 | <code>cp libhook_overwind.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1565 | <code>cd ../test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1566 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1567 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1568 | <code>vim ../../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1569 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1570 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1571 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1572 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1573 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1574 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1575 | <code>vim ../../src/impl/hook_overwind_detector_impl.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1576 | <code>cd ../../build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1577 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1578 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1579 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1580 | <code>cp libhook_overwind.so ../test/lib/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |
| 1581 | <code>cd ../test/build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1582 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1583 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1584 | <code>rm -r *</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1585 | <code>cmake ..</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1586 | <code>make</code> | 配置、编译或调用工程构建脚本；history 不包含退出码，不能据此确认成功。 |
| 1587 | <code>./test_hook_overwind </code> | 运行吊钩过卷测试程序，不能凭命令确认判定正确。 |
| 1588 | <code>vim ../src/hook_overwind_test.cpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1589 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1590 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1591 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1592 | <code>cd src/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1593 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1594 | <code>cd common/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1595 | <code>vim spdloger.hpp </code> | 打开源码/头文件/CMake 进行查看或编辑；不能从 history 得知具体修改。 |
| 1596 | <code>cd ../../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1597 | <code>cd ../</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1598 | <code>cd hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1599 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1600 | <code>cd 3rdparty/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1601 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1602 | <code>cd spdlog/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1603 | <code>ls -l</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1604 | <code>ls -l sinks/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1605 | <code>exit</code> | 退出当前 shell。 |
| 1606 | <code>cd crane_vision/hook_config_rec/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1607 | <code>cp CMakeLists.txt ../hook_overwind/</code> | 复制源码、头文件、CMake 或新编译库到测试目录；成功与否未记录。 |

<a id="history-1608"></a>
## 1608～1668：EVS 摄像头、人员界面与录屏

包含目录整理、摄像头格式与占用查询、YOLOX 摄像头测试和 Qt 界面启动。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 1608 | <code>mv *.mp4 ./evs/</code> | 移动/重命名文件或目录。 |
| 1609 | <code>cd evs/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1610 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1611 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1612 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1613 | <code>ls -lh evs_build</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1614 | <code>cd evs_build/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1615 | <code>du -sh</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1616 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1617 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1618 | <code>ls video_recording/</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1619 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1620 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1621 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1622 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1623 | <code>rm v4l2_detect_evs.*</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1624 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1625 | <code>rm -rf out*</code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1626 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1627 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1628 | <code>media-ctl -p</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1629 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1630 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1631 | <code>passwd root</code> | 切换用户或修改密码；后续目标用户的历史不在本次 hytop .bash_history 中。 |
| 1632 | <code>sudo passwd root</code> | 尝试修改 root 密码；密码本身不在 history 中，也不能判断是否成功。 |
| 1633 | <code>su</code> | 切换用户或修改密码；后续目标用户的历史不在本次 hytop .bash_history 中。 |
| 1634 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1635 | <code>rm /home/emmc_testfile </code> | 删除匹配文件/目录；rm -r * 的作用范围取决于当时实际目录，不能重放。 |
| 1636 | <code>df -h</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1637 | <code>./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5</code> | 运行 YOLOX 摄像头功能链路；根据 --frames 处理 5/30 帧。 |
| 1638 | <code>v4l2-ctl -d /dev/video0 --list-formats-ext</code> | 查询摄像头参数/格式或使用情况；本历史中的 --all/--list-formats-ext 是查询。 |
| 1639 | <code>fuser /dev/video0</code> | 检查哪个进程占用摄像头设备。 |
| 1640 | <code>GST_DEBUG=2 ./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5</code> | 命令尝试或拼写/语法试探；不能仅凭 history 确认命令存在或执行成功。 |
| 1641 | <code>cd ~/evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1642 | <code>./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5</code> | 运行 YOLOX 摄像头功能链路；根据 --frames 处理 5/30 帧。 |
| 1643 | <code>cd ~/evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1644 | <code>./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 5</code> | 运行 YOLOX 摄像头功能链路；根据 --frames 处理 5/30 帧。 |
| 1645 | <code>./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 30</code> | 运行 YOLOX 摄像头功能链路；根据 --frames 处理 5/30 帧。 |
| 1646 | <code>gdb ./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 30</code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 1647 | <code>gdb --args ./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 30</code> | 调试可执行文件；带目标参数应使用 gdb --args。 |
| 1648 | <code>./evs_task_cam --task eval_yolox.yaml --config camera_config.json --channel 0 --frames 30</code> | 运行 YOLOX 摄像头功能链路；根据 --frames 处理 5/30 帧。 |
| 1649 | <code>chmod +x evs_detect_ui </code> | 修改可执行权限；末段用于 UI 和相机程序。 |
| 1650 | <code>./evs_detect_ui </code> | 启动人员检测 Qt 界面。 |
| 1651 | <code>DISPLAY=:0 /home/hytop/uitest/uitest</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1652 | <code>DISPLAY=:0 ./evs_detect_ui </code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1653 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1654 | <code>export DISPLAY=:0</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1655 | <code>exec ./evs_detect_ui</code> | 用目标 UI 程序替换当前 shell；不同于普通后台启动。 |
| 1656 | <code>DISPLAY=:0 ffmpeg -y -f x11grab -framerate 25 -video_size 1920x1080 -i :0.0 -t 10 -c:v libx264 -preset ultrafast -pix_fmt yuv420p /tmp/screen.mp4</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1657 | <code>DISPLAY=:0 ffmpeg -y -f x11grab -framerate 25 -video_size 1920x1080 -i :0.0 -t 10 -c:v h264_rkmpp -b:v 4M -pix_fmt nv12 /tmp/screen.mp4</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1658 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1659 | <code>export DISPLAY=:0</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1660 | <code>exec ./evs_detect_ui</code> | 用目标 UI 程序替换当前 shell；不同于普通后台启动。 |
| 1661 | <code>DISPLAY=:0 XAUTHORITY=/home/hytop/.Xauthority gst-launch-1.0 -q ximagesrc use-damage=false num-buffers=250   ! video/x-raw,framerate=25/1 ! videoconvert   ! x264enc tune=zerolatency bitrate=4000 ! mp4mux   ! filesink location=/home/hytop/uitest/screen.mp4</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1662 | <code>export DISPLAY=:0</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1663 | <code>./evs_detect_ui</code> | 启动人员检测 Qt 界面。 |
| 1664 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1665 | <code>./evs_detect_ui</code> | 启动人员检测 Qt 界面。 |
| 1666 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1667 | <code>export DISPLAY=:0</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1668 | <code>./evs_detect_ui</code> | 启动人员检测 Qt 界面。 |

<a id="history-1669"></a>
## 1669～1694：冰雪模型文件与冰雪界面

包含列出模型、打包、视频协商和启动冰雪界面；模型文件存在不等于界面使用了该模型。

| 文件行号 | 命令 | 意图/含义 |
|---:|---|---|
| 1669 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1670 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1671 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1672 | <code>ls ice_rk3588.rknn </code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1673 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1674 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1675 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1676 | <code>tar zcvf evs_onboard.tar.gz evs</code> | 打包源码/部署目录；evs_onboard.tar.gz 的存在与位置需另查。 |
| 1677 | <code>cd evs/test_pics/</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1678 | <code>pwd</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1679 | <code>v4l2-ctl -d /dev/video0 --all &#124; grep -A2 "Width/Height"</code> | 查询摄像头参数/格式或使用情况；本历史中的 --all/--list-formats-ext 是查询。 |
| 1680 | <code>gst-launch-1.0 -v v4l2src device=/dev/video0 num-buffers=1 ! fakesink 2&gt;&amp;1 &#124; grep -o 'width=(int)[0-9]*, height=(int)[0-9]*'</code> | GStreamer/FFmpeg 摄像头协商或桌面录屏；带 filesink/输出路径时会写文件，本轮未执行。 |
| 1681 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1682 | <code>DISPLAY=:0 ./evs_ice_ui</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1683 | <code>chmod +x evs_ice_ui </code> | 修改可执行权限；末段用于 UI 和相机程序。 |
| 1684 | <code>DISPLAY=:0 ./evs_ice_ui</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1685 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1686 | <code>chmod +x evs_ice_cam </code> | 修改可执行权限；末段用于 UI 和相机程序。 |
| 1687 | <code>DISPLAY=:0 ./evs_ice_ui</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1688 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1689 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1690 | <code>DISPLAY=:0</code> | 设置环境变量或为该次 GUI 命令指定本机显示 :0；是否继承依赖 export/命令写法。 |
| 1691 | <code>cd ..</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1692 | <code>ls</code> | 查看文件、系统状态、硬件参数或依赖信息；具体对象见命令。 |
| 1693 | <code>cd evs</code> | 切换工作目录；是否切换成功及后续实际目录需终端输出确认。 |
| 1694 | <code>./evs_ice_ui </code> | 启动冰雪 Qt 界面；界面会调用 evs_ice_cam，非直接运行六分类模型。 |
