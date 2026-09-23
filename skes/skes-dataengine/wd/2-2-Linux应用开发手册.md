# Linux 应用开发手册

## 文档基本信息
- **文档标题**：Linux 应用开发手册
- **厂商**：广州创龙科技（Tronlong）
- **版本号**：V1.0
- **发布日期**：2025/04/03（修订记录：V1.0 初始版本）
- **总页数**：48 页
- **适用平台**：RK3576 评估板（TL3576-EVM，文中终端提示符为 `root@RK3576-Tronlong`）
- **配套资料路径**：产品资料 “4-软件资料\Demo\” 下的 `base-demos`（Linux 常用开发案例）和 `python-demos`（Python 开发案例）目录
- **联系方式**：官网 www.tronlong.com，技术论坛 www.51ele.net，技术邮箱 support@tronlong.com

### 文档整体结构（目录）
1. 前言
2. 第 1 章 Linux 常用开发案例
   - 1.1 led_flash 案例
   - 1.2 key_demo 案例
   - 1.3 can_echo 案例
   - 1.4 tcp_udp 案例
   - 1.5 uart_rw 案例
   - 1.6 devmem_rw 案例
3. 第 2 章 Python 开发案例
   - 2.1 led_flash 案例
   - 2.2 key_demo 案例
4. 更多帮助

---

## 逐章节内容摘要

### 前言
- 文档涉及的开发案例位于产品资料 “4-软件资料\Demo\” 路径下的 `base-demos` 和 `python-demos` 目录。
- `base-demos` 目录存放 Linux 常用开发案例：案例 `bin` 目录存放可执行文件，案例 `src` 目录存放源码。
- `python-demos` 目录存放 Python 开发案例，脚本文件无需编译，可基于厂商提供的文件系统直接运行。
- 重新编译 Linux 常用开发案例的流程：先参考《Linux 系统使用手册》正确安装 LinuxSDK，构建适配评估板的交叉编译工具链；然后将对应案例 `src` 目录拷贝至 Ubuntu 工作目录；在 `src` 目录下配置交叉编译工具链并执行 `make` 完成编译，编译完成后在当前目录生成可执行文件。
- 关键命令示例（Host 端，Ubuntu）：
  - `source /home/tronlong/RK3576/rk3576_linux6.1_release/buildroot/output/rockchip_rk3576/host/environment-setup`（加载 buildroot 交叉编译环境，导出 CC/LD/CFLAGS、ARCH/CROSS_COMPILE/KERNELDIR 等变量，并提供 `cmake` 别名）
  - `make`（执行编译，图 1 显示用 `aarch64-buildroot-linux-gnu-gcc -Wall led_flash.c -o led_flash` 生成可执行文件）
- 备注：LinuxSDK 版本号以实际情况为准。

### 第 1 章 Linux 常用开发案例

#### 1.1 led_flash 案例
- **1.1.1 案例功能**：通过向评估底板用户可编程指示灯 LED 设备节点反复交替写入 1、0 数值，实现 LED 闪烁效果。LED 点亮与熄灭时间均为 0.5s。程序流程：开始→解析参数→控制 LED 亮灭→判断 ctrl+c→结束。LED 设备节点为 `/sys/class/leds/user-ledX/` 目录下的 `brightness`。`ls /sys/class/leds/` 可见 disk、heartbeat、mmc0::、user-led0、user-led1 等；`ls /sys/class/leds/user-led0` 含 brightness、device、max_brightness、power、subsystem、trigger、uevent。
- **1.1.2 操作说明**：将 bin 目录下可执行程序 `led_flash` 拷贝至评估板文件系统，在所在目录执行运行，即可看到 LED 以 0.5s 间隔闪烁，同时串口终端打印系统全部 LED 设备信息和当前控制的 LED 设备信息。
  - `./led_flash -h`：打印帮助（Usage: led_flash [options]；-n/--number 指定 LED 数量，范围 1~4；-v/--version 显示版本信息；-h/--help 显示帮助）。
  - `./led_flash -n 2`：控制 2 个 LED 闪烁，打印 System leds 列表与 Flashing leds（user-led0、user-led1）。
- **1.1.3 关键代码**：
  - （1）预定义 LED 数组 `g_leds[]`，包含 `/sys/class/leds/user-led0`~`user-led3`。数组信息必须为系统已有 LED，否则程序报错。
  - （2）LED 亮灭操作与时间间隔：循环中向 brightness 写 1（点亮）/写 0（熄灭），通过 `system()` 执行 `echo` 命令，每次保持 500ms（`usleep(500*1000)`）。

#### 1.2 key_demo 案例
- **1.2.1 案例功能**：监听用户输入按键 USER1(KEY4)、USER2(KEY5) 的状态，检测按键事件。案例获取按键事件后进行键值匹配，再进行事件处理（信息打印）。流程：开始→参数解析校验→监听按键事件→事件处理→判断 ctrl+c→结束。
- **1.2.2 操作说明**：以 USER1(KEY4) 为例测试，测试 USER2(KEY5) 需修改对应设备节点。按键事件号对应关系（表 1）：USER1(KEY4)→`/dev/input/event5`，USER2(KEY5)→`/dev/input/event8`。通过 `cat /proc/bus/input/devices` 查看按键对应设备节点（图 9 中 Name="adc-keys2" 对应 Handlers=kbd event5）。
  - `./key_demo -h`：打印帮助（-d/--device 指定设备，-v/--version 版本，-h/--help 帮助）。
  - `./key_demo -d /dev/input/event5`：监听 USER1，按键后打印 "User key pressed!"。
- **1.2.3 关键代码**：
  - （1）定义按键：`typedef enum { KEY_CODE_NONE = 0, KEY_CODE_USER1, KEY_CODE_USER2 } KeyCode;`
  - （2）监听按键事件 `check_button_pressed(int fd)`：用 `select()` 等待按键按下/释放，`read()` 读取 `struct input_event`，根据 `buf.code` 判断（KEY_PROG1→USER1，KEY_PROG2→USER2），`buf.value==1` 表示按下。
  - （3）循环监听：`while(!g_quit)` 中调用 check_button_pressed，根据返回的 key_code switch 打印 "Key user1 pressed!"/"Key user2 pressed!"。

#### 1.3 can_echo 案例
- **1.3.1 案例功能**：绑定一个 CAN 总线接口用于接收数据，当 CAN 端口接收到数据后，将数据重新发送到 CAN 总线接口（回环回显）。流程包含参数解析校验、打印 help/version、绑定 CAN 接口接收数据、接收数据、将接收数据重新发送、判断 Ctrl+c。
- **1.3.2 操作说明**：
  - 使用 USB-CAN 分析仪（广成科技 USBCAN-II FD）连接评估板 CAN-FD1 或 CAN-FD2 接口至 PC USB 接口。以 CAN-FD1（设备节点 can0）为例；CAN-FD2 对应 can1。
  - 连接关系（表 2）：CAN-FD1 接口 H1 端子→分析仪 H，L1 端子→L，GNDI 端子→G。
  - 用 GCAN Tools 调试软件：选择设备类型 USBCAN-FD，点击“打开设备”，选 CAN1 或 CAN2，设置波特率 1000K、数据波特率 5000K，点“确定”。进入发送界面取消 "CAN FD" 选项，连接成功显示 Connected。
  - 评估板配置 CAN0 命令（波特率 1Mbps、数据波特率 5Mbps）：
    - `ip link set can0 down`
    - `ip link set can0 type can bitrate 1000000 dbitrate 5000000 fd on`
    - `ip link set can0 up`
  - `./can_echo --help`：Usage: can_echo `<can-interface>` [`<can-interface-out>`] [options]；将 can-interface 收到的所有消息发送到 can-interface-out；省略时用 can-interface 发送。选项：-f/--family 协议族(默认 PF_CAN=29)，-t/--type socket 类型(默认 SOCK_RAW=3)，-p/--protocol CAN 协议(默认 CAN_RAW=1)，-v 详细输出，-h 帮助，--version 版本。
  - `./can_echo -v can0`：绑定 CAN-FD1，接收 GCAN Tools 数据并重发。在 GCAN Tools 输入数据点发送，可见两帧（一发一收）。终端打印 `interface-in = can0, interface-out = can0, family = 29, type = 3, proto = 1`，收到数据如 `0000: [8] 00 01 02 03 04 05 06 07`。GCAN 接收区显示发送成功(000)和接收(001)两帧。
  - 退出：可按 "Ctrl+Z" 暂停程序，执行 `killall -9 can_echo` 退出。
- **1.3.3 关键代码**：
  - （1）使用 socket 监听 CAN 接口：`for` 循环中 `socket(family, type, proto)` 创建套接字，设置 `addr[i].can_family`，`strcpy(ifr[i].ifr_name, intf_name[i])`，`ioctl(s[i], SIOCGIFINDEX, &ifr[i])` 获取接口索引，`addr[i].can_ifindex = ifr[i].ifr_ifindex`，`bind()` 绑定。
  - （2）将 CAN 接收数据重新发送：`while(running)` 中 `read(s[0], &frame, sizeof(frame))` 读取帧，verbose 模式打印 can_id（含 CAN_RTR_FLAG 判断远程请求）、can_dlc、data，`frame.can_id++` 后 `write(s[out], &frame, sizeof(frame))` 发出。

#### 1.4 tcp_udp 案例
- **1.4.1 案例功能**：实现客户端(client)与服务端(server)文本数据相互收发。包含 4 个程序：
  - （1）tcp_server：TCP 服务端测试程序；
  - （2）tcp_client：TCP 客户端测试程序；
  - （3）udp_server：UDP 服务端测试程序；
  - （4）udp_client：UDP 客户端测试程序。
  - TCP 流程（图 27）：启动→绑定 IP/端口→监听→等待客户端接入→创建子进程→（读用户输入并发送 / 接收并打印）→退出。UDP 流程（图 28）类似，服务端用线程。
- **1.4.2 操作说明**：服务端和客户端均可在评估板、PC Ubuntu 上运行。本章采用评估板本地回环测试（127.0.0.1），bin 目录 4 个文件均为 ARM 端可执行程序。
  - Ubuntu 用 OpenSSH 登录评估板：`sudo ssh root@192.168.13.27`（IP 按实际修改）。
  - 评估板启用本地回环：`ifconfig lo up`。
  - **TCP 通信测试**：`./tcp_server 2233`（服务端，2233 为端口号）；`./tcp_client 127.0.0.1 2233`（客户端，端口需与服务端一致）。服务端输入 "Tronlong" 客户端显示；客户端输入 "Hello Tronlong" 服务端显示。
  - **UDP 通信测试**：`./udp_server 2233`；`./udp_client 127.0.0.1 2233`。UDP 客户端不会自动连接服务端，需客户端先向服务端发送信息（服务端收到前无法得知客户端存在）。
  - 如需在 PC Ubuntu 运行，将 src 源码目录拷贝至 Ubuntu 工作目录，执行 `make` 生成 x86 端可执行文件。在不同终端运行命令类似，但 127.0.0.1 需用服务器 IP 替代。
    - `make clean`（清理）
    - `make CC=gcc`（用本机 gcc 编译，生成 x86-64 可执行文件；图 35 中 `file tcp_client` 显示 ELF 64-bit x86-64）。
- **1.4.3 关键代码**（以 TCP 通信程序为例）：
  - （1）tcp_client.c：注意源码中 `struct sockaddr_in`、`socket`、`connect`、`fgets`、`send`、`recv` 等数据结构和系统调用的使用。
  - （2）tcp_server.c：注意源码中 `struct sockaddr_in`、`socket`、`connect`、`fgets`、`send`、`recv`、`bind`、`listen`、`accept` 等数据结构和系统调用的使用。

#### 1.5 uart_rw 案例
- **1.5.1 案例功能**：实现评估板串口读写功能。串口初始化时设置波特率、模式、数据位、停止位等参数，通过设备文件描述符对串口进行读写操作。流程：开始→参数解析校验→初始化串口→读写操作→读写校验→判断 Ctrl+c→结束。
  - 支持的串口及设备节点（表 3）：RS232 UART8→ttyS8；RS485 UART1→ttyS1；RS485 UART2→ttyS2。
- **1.5.2 操作说明**：
  - 通过 RS232 交叉串口母母线、USB 转 RS232 公头串口线，将评估板 RS232 UART8 调试串口连接至 PC USB 接口。
  - `./uart_rw -h`：帮助。选项：-d/--device 设备如 '/dev/ttyS0'；-r/--read 读；-w/--write 写；-l/--loopback 回环测试；-s/--size 读大小；-v/--version 版本；-h/--help 帮助。示例：`./uart_rw -d /dev/ttyS1 -r -s 256`、`./uart_rw -d /dev/ttyS1 -w -s 1024`、`./uart_rw -d /dev/ttyS1 -l -s 1024`。
  - **（1）RS232 UART8 测试**：
    - 调试串口 USB TO UART0 为 COM11，RS232 UART8 为 COM12。COM11/COM12 波特率均设 115200，8N1，无校验位。
    - a) 评估板接收：`./uart_rw -d /dev/ttyS8 -r -s 8`，评估板等待接收上位机发送的数据。在 RS232 UART8 串口终端 View→Command(Chat)Window 输入数据（如 "Tronlong"）回车，评估板终端打印接收到的数据（recv: Tronlong, size: 8）。
    - 备注：评估板启动后首次执行 uart_rw 会出现打印 "of_dma_request_slave_channel..."（dma-names property missing，failed to request DMA, use interrupt mode），不影响使用，请忽略。
    - b) 评估板发送：`./uart_rw -d /dev/ttyS8 -w -s 8`，评估板通过 RS232 UART8 发送数据至上位机（数据已在程序中定义为 "01234567"，send size: 8），UART8 串口终端打印接收到的数据。
  - **（2）RS485 串口测试**：
    - 使用 RS232 转 RS485 模块、USB 转 RS232 公头串口线，将评估板 RS485 UART1 或 UART2 连接至 PC USB。
    - 连接关系（表 4）：RS485 UART1(ttyS1)→模块 485+→A1，485-→B1，GND→GNDI；RS485 UART2(ttyS2)→A2/B2/GNDI。
    - 以 RS485 UART1（ttyS1）为例。a) 接收：`./uart_rw -d /dev/ttyS1 -r -s 8`；b) 发送：`./uart_rw -d /dev/ttyS1 -w -s 8`。步骤与现象同 UART8，仅设备节点由 ttyS8 改为 ttyS1。
- **1.5.3 关键代码**：
  - （1）串口初始化函数 `init_serial(int *fd, const char *dev)`：`open(dev, O_RDWR)` 打开串口；`tcgetattr` 获取属性；`opt.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG)`（非规范、关回显）；`opt.c_oflag &= ~OPOST`；清 CSIZE 后设数据位；`opt.c_cflag &= ~CRTSCTS`（无硬件流控）；`opt.c_cflag |= CS8`（8 位数据）；`opt.c_cflag &= ~CSTOPB`（1 位停止位）；`opt.c_iflag |= IGNPAR`（无校验）；输出模式 `c_oflag=0`，`c_lflag=0`；`cfsetispeed/cfsetospeed(&opt, B115200)` 设波特率；`tcflush(*fd, TCIFLUSH)` 清缓存；`tcsetattr(*fd, TCSANOW, &opt)` 应用设置。
  - （2）串口读写函数：
    - `serial_write(int *fd, const char *data, size_t size)`：`write(*fd, data, size)`，失败时 perror 并 `tcflush(*fd, TCOFLUSH)`。
    - `serial_read(int *fd, char *data, size_t size)`：用 `select()`（5 秒超时）+ `read()` 循环读取，直至读满 size 或读到 0，返回 read_size。
    - `run_read_mode(char *dev, size_t size)`：init_serial 后 malloc 缓冲，调用 serial_read，打印 "recv: %s\nsize: %d"。
    - `run_write_mode(char *dev, size_t size)`：循环生成字符 '0'+i（i 取 0~7 循环），调用 serial_write 直至写满 size，打印 "send size"。
  - （3）回环测试 `run_loopback_test(char *dev, size_t size)`：串口缓冲默认 2k~4k，分块（每块 ≤1024 字节）；`memset(write_buf, rand()%26+65, ...)` 生成随机数据；serial_write 后 `usleep(90000)` 延时（延时 > 1024/115200*1000000），再 serial_read，`memcmp` 比较读写一致性，不一致打印 "Result: Test failed"，否则 "Result: Test pass"。
  - （4）相关逻辑：根据 mode（READ/WRITE/LOOPBACK）分别调用 run_read_mode/run_write_mode/run_loopback_test。

#### 1.6 devmem_rw 案例
- **1.6.1 案例说明**：通过 PCIe 总线对外部设备进行数据读写测试，同时校验数据和计算读写速率。流程：开始→分析参数→将物理地址映射到虚拟地址→判断模式（Test/Read/Write）→（生成测试数据写读 / 从设备内存读取 / 将数据写入设备内存）→打印信息→结束。
  - 案例资料位于 "4-软件资料\Demo\base-demos\devmem_rw\"。目录结构（表 5）：arm/bin（ARM 可执行程序）、arm/src（应用程序源码）、fpga/bin（FPGA 可执行程序）、fpga/project（FPGA 工程源码，源码仅用于测试评估，暂无案例说明）。
  - 程序流程：a) ARM 端通过 PCIe 将数据写入 FPGA BRAM；b) ARM 端通过 PCIe 从 FPGA BRAM 读取数据；c) 判断读写数据正确性并计算读写速率。
  - 程序原理（ARM 端）：a) 使用 mmap 函数对物理地址进行转换；b) 可根据指定访问类型按字节(Byte)、按半字(Halfword)访问。
- **1.6.2 案例测试**：
  - 使用 M.2 Key M 转 PCI-E 4X 转接板（华高智 PCI-E 4X TO M.2 M），将 TL3576-EVM 的 M.2 PCIe NVMe 接口与 TLA7-EVM 评估板 PCIe 接口连接。
  - 将 TLA7-EVM 上电启动，通过下载器加载运行 fpga\bin\ 下的可执行程序（实现 PCIe Endpoint 功能，采用 PCIe 同源时钟模式）。
  - 备注：为确保 Linux 端识别成功 PCIe EP 设备，请先将 TLA7-EVM 上电，等待 Done 指示灯(LED4)亮起后，再将 TL3576-EVM 上电。
  - 将 arm\bin\ 下的 devmem_rw 拷贝至文件系统，执行 `./devmem_rw -h` 查看参数。**参数解析**：
    - `-t`：测试模式，先写后读，并检验数据准确性。
    - `-r`：读模式，从 `-a <address>` 指定的内存地址处，读出 `-s <size>` 指定长度的数据（字节为单位）。
    - `-w`：写模式，从 `-f <file>` 指定的文件获取数据，写入 `-a <address>` 指定的地址。
    - `-a <address>`：指定内存空间地址。
    - `-s <size>`：指定数据长度（字节为单位）。
    - `-o <byte|halfword>`：以字节或半字方式访问内存空间。
    - `-f <file>`：指定文件，用于写模式测试。
    - 示例：`./devmem_rw -t -a 0x660000 -s 1024 -o halfword`；`-r -a 0x660000 -s 1024 -o halfword`；`-w -a 0x660000 -s 1024 -o halfword -f ./data.dat`。
  - `lspci -s 01:00.0 -vv`：查看 PCIe EP 连接状态。图 62 显示 Xilinx Device 7021，Region 0 0x20200000、Region 1 0x20210000（32-bit non-prefetchable，size=64K），LnkSta Speed 5GT/s Width x1。BYPASS 数据空间起始地址 0x20210000，大小 64KByte。
  - 使能 PCIe 设备：`echo 1 > /sys/class/pci_bus/0000\:01/device/0000\:01\:00.0/enable`。
  - 读写测试：
    - `./devmem_rw -t -a 0x20210000 -s 65536 -o halfword`：半字方式，写速率 39.038MB/s，读速率 1.275MB/s。
    - `./devmem_rw -t -a 0x20210000 -s 65536 -o byte`：字节方式，写速率 19.501MB/s，读速率 0.638MB/s。
    - 打印 "Test pass!" 表示测试通过。备注：本案例仅验证功能，因按字节访问且数据量少，读写速率偏低。
- **1.6.3 案例编译（ARM 端）**：将 devmem_rw\arm\src\ 源码拷贝至 Ubuntu 工作目录，`source .../host/environment-setup` 配置交叉编译工具链，`make` 编译（图 66 用 `aarch64-buildroot-linux-gnu-gcc -c devmem_rw.c -Wall -g`、`-o devmem_rw devmem_rw.o -lpthread`）生成可执行文件。
- **1.6.4 关键代码（ARM 端）**：
  - （1）将内存设备地址映射至虚拟地址 `map_addr(int *fd, size_t length, off_t phy_addr, void **map_base, void **virt_addr)`：`open("/dev/mem", O_RDWR|O_SYNC)`；`pa_offset = phy_addr & ~(sysconf(_SC_PAGE_SIZE)-1)`；`mmap(NULL, length+phy_addr-pa_offset, PROT_READ|PROT_WRITE, MAP_SHARED, *fd, pa_offset)`；`*virt_addr = *map_base + phy_addr - pa_offset`。
  - （2）按操作方式读取数据 `read_data(void *virt_addr, void *data, int size, char *op_type)`：`gettimeofday` 计时；byte 模式按 uint8_t（8 位）逐个读取（out[j]=in[j]，实际从设备内存读取）；halfword 模式按 uint16_t（16 位）；计算耗时与速率并打印 "%d bytes (%.3f MB) read, %.3f ms, %.3f MB/s"。
  - （3）按操作方式写入数据 `write_data(void *virt_addr, void *data, int size, char *op_type)`：byte/halfword 模式将 data 写入虚拟地址（in[i]=out[i]，实际写入设备内存），计算耗时与速率打印 "writed"。

### 第 2 章 Python 开发案例
- 本章以两个简单案例演示 Python 使用方法。系统支持 **Python3.11**，相关库位于文件系统 `/usr/lib/python3.11/` 目录下（图 70 列出大量标准库 .pyc 文件，含 config-3.11-aarch64-linux-gnu 等）。

#### 2.1 led_flash 案例（Python）
- **2.1.1 案例功能**：通过向用户可编程指示灯 LED 设备节点反复交替写入 1、0，实现 LED 闪烁，点亮与熄灭时间均 0.5s。流程：开始→解析参数→打印 version/help 判断→led 设备>1 判断（否则查找所有 led 设备）→控制 led 亮灭→判断 ctrl+c→结束。LED 设备节点为 `/sys/class/leds/user-ledX/` 下的 brightness。
- **2.1.2 操作说明**：将 `led_flash.py` 脚本拷贝至评估板文件系统。
  - `./led_flash.py -h`：帮助（Usage: led_flash.py [options]；-v/--version 版本；-h/--help 帮助）。
  - `./led_flash.py`：运行脚本，LED 闪烁。打印 "find leds:" 列出 user-led0、user-led1，然后 "flash led ..."。可按 "Ctrl+C" 终止。
- **2.1.3 关键代码**：
  - （1）查找所有 LED 设备 `enumerate_led(self)`：`led_name = "user-led"`，遍历 `os.listdir(self.led_path)`，含 "user-led" 的加入 self.leds；若 `len(self.leds)==0` 返回 False；`self.leds.sort()` 排序；打印 "find leds:" 及每个 led；返回 True。
  - （2）控制 LED 亮灭 `flash_led(self)`：`led_num = len(self.leds)`；`while not Led.quit_flag` 循环；点亮 `os.system("echo 1 > %s/brightness" % self.leds[i])`，失败打印 Error；`time.sleep(0.5)`；熄灭 `os.system("echo 0 > %s/brightness" ...)`；再 `time.sleep(0.5)`。

#### 2.2 key_demo 案例（Python）
- **2.2.1 案例功能**：监听用户输入按键 USER1(KEY4)、USER2(KEY5) 状态，检测按键事件。流程：开始→解析参数→打印 version/help 判断→打开 key 设备（成功判断）→监听按键按下→串口终端打印对应按键提示→判断 ctrl+c→结束。
- **2.2.2 操作说明**：以 USER1(KEY4) 为例，测试 USER2(KEY5) 需修改设备节点。按键事件号（表 6）：USER1(KEY4)→`/dev/input/event5`，USER2(KEY5)→`/dev/input/event8`。`cat /proc/bus/input/devices` 查看（adc-keys2 对应 event5）。
  - `./key_demo.py -h`：帮助（-d/--device 设备如 /dev/input/event0；-v 版本；-h 帮助）。
  - `./key_demo.py -d /dev/input/event5`：运行，打印 "Please press the key to test."，按键后打印 "User key pressed!"。可按 "Ctrl+C" 终止。
- **2.2.3 关键代码**：
  - （1）打开按键设备 `open_device(self, key_dev)`：`try` 中 `self.key_dev = evdev.InputDevice(key_dev)` 返回 True；异常打印并返回 False。
  - （2）监听按键事件 `listen_key_pressed(self)`：打印提示；`while not KeyDevice.quit_flag` 循环 `event = self.key_dev.read_one()`，无事件则 continue；`event.type == evdev.ecodes.EV_KEY` 时，`event.code == KEY_PROG1 and event.value == 1` 打印 "User key pressed!"，`event.code == KEY_PROG2 and event.value == 1` 同样打印（event.value: 1 按下，0 释放）；`@classmethod def stop(cls)` 设置 `cls.quit_flag = True`。

### 更多帮助
- 销售邮箱：sales@tronlong.com；技术邮箱：support@tronlong.com；创龙总机：020-8998-6280；技术热线：020-3893-9734；官网：www.tronlong.com；技术论坛：www.51ele.net；官方商城：tronlong.tmall.com。

---

## 关键 API / 命令清单

### Shell / 终端命令
| 命令 | 作用 | 参数/说明 |
| --- | --- | --- |
| `source .../host/environment-setup` | 加载 buildroot 交叉编译环境 | 导出 CC/LD/CFLAGS、ARCH/CROSS_COMPILE/KERNELDIR，提供 cmake 别名 |
| `make` / `make clean` / `make CC=gcc` | 编译 / 清理 / 用本机 gcc 编译生成 x86 程序 | — |
| `ls /sys/class/leds/` | 查看系统 LED 设备节点 | 含 user-led0~led1、heartbeat 等 |
| `echo 1 > /sys/class/leds/user-ledX/brightness` | 点亮 LED（写 0 熄灭） | brightness 节点 |
| `cat /proc/bus/input/devices` | 查看输入设备及事件号 | adc-keys2 → event5 |
| `ip link set can0 down` | 关闭 CAN0 接口 | 配置前需先 down |
| `ip link set can0 type can bitrate 1000000 dbitrate 5000000 fd on` | 设置 CAN0 仲裁波特率 1Mbps、数据波特率 5Mbps、使能 CAN-FD | bitrate/dbitrate/fd |
| `ip link set can0 up` | 启用 CAN0 接口 | — |
| `killall -9 can_echo` | 强制结束 can_echo 进程 | 配合 Ctrl+Z 暂停后退出 |
| `sudo ssh root@192.168.13.27` | OpenSSH 登录评估板 | IP 按实际修改 |
| `ifconfig lo up` | 启用本地回环接口 | TCP/UDP 本地回环测试用 |
| `lspci -s 01:00.0 -vv` | 查看 PCIe EP 设备详情 | 显示 Region 地址、链路速率 |
| `echo 1 > /sys/class/pci_bus/0000\:01/device/0000\:01\:00.0/enable` | 使能 PCIe 设备 | — |

### 案例程序命令
| 程序 | 命令示例 | 作用 |
| --- | --- | --- |
| led_flash | `./led_flash -n 2` / `-h` / `-v` | 控制 N(1~4) 个 LED 闪烁 |
| key_demo | `./key_demo -d /dev/input/event5` | 监听指定按键事件 |
| can_echo | `./can_echo -v can0` | 绑定 CAN 接口接收并回环转发；-f/-t/-p/-v 选项 |
| tcp_server/tcp_client | `./tcp_server 2233` / `./tcp_client 127.0.0.1 2233` | TCP 服务端/客户端，端口需一致 |
| udp_server/udp_client | `./udp_server 2233` / `./udp_client 127.0.0.1 2233` | UDP 服务端/客户端 |
| uart_rw | `./uart_rw -d /dev/ttyS8 -r -s 8` / `-w -s 8` / `-l -s 1024` | 串口读/写/回环测试 |
| devmem_rw | `./devmem_rw -t -a 0x20210000 -s 65536 -o halfword` | PCIe 内存读写测试，-t/-r/-w/-a/-s/-o/-f |
| led_flash.py | `./led_flash.py` | Python 版 LED 闪烁 |
| key_demo.py | `./key_demo.py -d /dev/input/event5` | Python 版按键监听 |

### C 关键 API / 系统调用
- **文件/IO**：`open`、`read`、`write`、`close`、`malloc/free`、`system`、`snprintf`、`usleep`。
- **select 多路复用**：`fd_set`、`FD_ZERO`、`FD_SET`、`FD_ISSET`、`select(maxfd+1, &set, NULL, NULL, &timeout)`。
- **输入事件**：`struct input_event`（含 type/code/value），KEY_PROG1/KEY_PROG2。
- **CAN socket**：`socket(family, type, proto)`（PF_CAN=29, SOCK_RAW=3, CAN_RAW=1）、`struct sockaddr_can`、`struct ifreq`、`ioctl(s, SIOCGIFINDEX, &ifr)`、`bind`、`can_id`、`CAN_RTR_FLAG`、`can_dlc`。
- **TCP/UDP socket**：`struct sockaddr_in`、`socket`、`connect`、`bind`、`listen`、`accept`、`send`、`recv`、`fgets`。
- **串口 termios**：`struct termios`、`tcgetattr`、`tcsetattr(TCSANOW)`、`cfsetispeed/cfsetospeed(B115200)`、`tcflush(TCIFLUSH/TCOFLUSH)`，标志位 ICANON/ECHO/ECHOE/ISIG/OPOST/CSIZE/CRTSCTS/CS8/CSTOPB/IGNPAR。
- **内存映射**：`mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, pa_offset)`、`sysconf(_SC_PAGE_SIZE)`、`/dev/mem`、`gettimeofday`。

### Python 关键 API
- `os.listdir`、`os.path.join`、`os.system("echo ... > .../brightness")`、`time.sleep`、`list.sort()`。
- `evdev.InputDevice(key_dev)`、`key_dev.read_one()`、`evdev.ecodes.EV_KEY`、`evdev.ecodes.KEY_PROG1/KEY_PROG2`、`@classmethod`。

---

## 示例代码说明

1. **图 5（led_flash g_leds 数组）**：预定义 `static char *g_leds[]` 含 user-led0~led3 路径，程序遍历该数组操作 LED，数组项必须是系统真实存在的 LED，否则报错。
2. **图 6（LED 亮灭循环）**：`while(!g_quit)` 中两个 for 循环分别向各 LED brightness 写 1（点亮）/写 0（熄灭），用 `snprintf` 拼 echo 命令后 `system()` 执行，每阶段 `usleep(500*1000)` 保持 500ms，实现 0.5s 闪烁。失败时 fprintf + exit。
3. **图 11（按键枚举）**：`typedef enum { KEY_CODE_NONE=0, KEY_CODE_USER1, KEY_CODE_USER2 } KeyCode;` 定义按键值。
4. **图 12（check_button_pressed）**：`select(fd+1,...)` 阻塞等待按键，`read` 读 input_event，switch(buf.code)：KEY_PROG1 且 value==1 返回 USER1，KEY_PROG2 返回 USER2，否则 NONE。
5. **图 13（循环监听）**：`while(!g_quit)` 调 check_button_pressed，<0 则 continue，switch 打印 "Key user1/user2 pressed!"。
6. **图 25（CAN socket 绑定）**：for 循环创建 socket，设置 can_family，strcpy 接口名，ioctl 取 ifindex，赋给 addr.can_ifindex，bind 绑定。多个接口可循环绑定。
7. **图 26（CAN 回环转发）**：`while(running)` read 一帧；verbose 时打印 can_id（判断 CAN_RTR_FLAG 远程请求）、can_dlc、各 data 字节；`frame.can_id++` 后 write 到输出接口，实现回显并修改 ID。
8. **图 50/51（init_serial）**：open 串口、tcgetattr 后逐项配置 termios（关闭规范模式/回显/流控、设 8N1、设波特率 115200、清缓存、tcsetattr 应用）。注意先清 CSIZE 再设 CS8。
9. **图 52（serial_write）**：write 数据，失败 perror 并 tcflush 清输出缓存。
10. **图 53（serial_read）**：select 5 秒超时循环读取，case -1 select 错误、case 0 超时重试、default FD_ISSET 后 read 累加，读满或读到 0 退出。
11. **图 54/55（run_read_mode/run_write_mode）**：读模式 malloc 缓冲读取打印；写模式循环生成 '0'~'7' 字符写满 size。
12. **图 56/57（run_loopback_test）**：分块（≤1024 字节）生成随机数据写入，`usleep(90000)` 等待传输（延时需 > 1024/115200*1e6），再读回 memcmp 校验，失败 goto release。
13. **图 58（mode 分支）**：switch(mode) 根据 READ/WRITE/LOOPBACK 调用对应函数，返回相应条件码。
14. **图 67（map_addr）**：open /dev/mem，按页对齐 pa_offset，mmap 映射，计算 virt_addr = map_base + (phy_addr - pa_offset)。
15. **图 68/69（read_data/write_data）**：gettimeofday 计时，按 byte(uint8_t)/halfword(uint16_t) 逐元素拷贝（in/out 指针即设备内存/用户数据），最后 timersub 计算耗时与 MB/s 速率。
16. **图 75（Python enumerate_led）**：os.listdir 过滤含 "user-led" 的节点，sort 排序，打印列表。
17. **图 76（Python flash_led）**：while not quit_flag 循环，os.system echo 1/0 控制亮灭，time.sleep(0.5) 间隔。
18. **图 82（Python open_device）**：try 中 evdev.InputDevice 打开按键设备，异常返回 False。
19. **图 83（Python listen_key_pressed）**：while not quit_flag 调 read_one，EV_KEY 且 code 为 KEY_PROG1/PROG2 且 value==1 时打印 "User key pressed!"；classmethod stop 置 quit_flag。

<!-- GENERATION_COMPLETE: 2026-06-24_22:45 -->
