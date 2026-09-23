# modem_mng 编译指南（EC200A / EG25G）

## 前置条件

- 已安装 CMake 3.11+（建议 3.27+ 可规避部分已知问题）
- 已安装对应平台的 Quectel SDK 工具链
- 已安装 `jq`（打包 OTA 镜像时需要）

---

## 第一步：修改工具链路径（首次使用必做）

两个 env-init 脚本里的 SDK 路径目前写的是其他机器上的路径，**必须先改成你本机的实际路径**。

### EC200A

编辑 `cross-profiles/ql-ol-crosstool-env-init-ec200a`，修改以下两行：

```bash
export QL_SDK_DIR=/home/xp/work/a200ec/fast_ec200a/ql-ol-extsdk-ec200acntar02a04m2g_ocpu/
export STAGING_DIR_HOST=/home/xp/work/a200ec/fast_ec200a/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain
```

改为你本机的实际 SDK 目录，例如：

```bash
export QL_SDK_DIR=/home/tronlong/sdk/ec200a/ql-ol-extsdk-ec200acntar02a04m2g_ocpu/
export STAGING_DIR_HOST=/home/tronlong/sdk/ec200a/ql-ec200a-1803e-gcc-8.4.0-v1-toolchain
```

> `QL_TOOLCHAIN_DIR` 等其他路径都是基于 `QL_SDK_DIR` 自动推导的，只需改上面两行。

### EG25G

编辑 `cross-profiles/ql-ol-crosstool-env-init-eg25g`，修改以下一行：

```bash
export QL_SDKPATH=/home/xp/work/eg25-g/ql-ol-sdk/ql-ol-crosstool
```

改为你本机的实际路径，例如：

```bash
export QL_SDKPATH=/home/tronlong/sdk/eg25g/ql-ol-sdk/ql-ol-crosstool
```

---

## 第二步：清理旧的构建目录

当前 `build/` 目录是为 RK3576 配置的，CMakeCache 已损坏，**必须删除后重建**：

```bash
cd /home/tronlong/lyp/code/rtms_sdk
rm -rf build
```

---

## 第三步：EC200A 编译步骤

### 3.1 加载工具链环境

```bash
cd /home/tronlong/lyp/code/rtms_sdk
source cross-profiles/ql-ol-crosstool-env-init-ec200a
```

验证环境已加载：

```bash
echo $QL_MODULE_PLATFORM   # 应输出 EC200A
echo $CC                   # 应输出 arm-openwrt-linux-gcc 的完整路径
```

### 3.2 CMake 配置

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cross-profiles/toolchain_armv7-a-ec200a.cmake \
    -DCMAKE_INSTALL_PREFIX=./package_install \
    -DWITH_RTMS_CORE=1
```

### 3.3 编译

```bash
make -j$(nproc)
```

### 3.4 安装（去除调试符号）

```bash
make install/strip
```

安装输出目录：`build/package_install/`

### 3.5 打包 OTA 升级包

```bash
cd ..   # 回到仓库根目录
./generate_img.sh build/package_install
```

脚本会交互式询问产品类型和平台类型，完成后在 `build/package_install/` 目录生成 `ir_ota_YYYYMMDD.img`。

---

## 第四步：EG25G 编译步骤

### 4.1 加载工具链环境

```bash
cd /home/tronlong/lyp/code/rtms_sdk
source cross-profiles/ql-ol-crosstool-env-init-eg25g
```

验证环境已加载：

```bash
echo $QL_MODULE_PLATFORM      # 应输出 EG25G
echo $QL_SDK_TARGET_SYSROOT   # 应输出 sysroot 的完整路径
```

### 4.2 CMake 配置

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cross-profiles/toolchain_armv7-a-eg25g.cmake \
    -DCMAKE_INSTALL_PREFIX=./package_install \
    -DWITH_RTMS_CORE=1
```

### 4.3 编译

```bash
make -j$(nproc)
```

### 4.4 安装（去除调试符号）

```bash
make install/strip
```

### 4.5 打包 OTA 升级包

```bash
cd ..
./generate_img.sh build/package_install
```

---

## 两个平台交替编译

每次切换平台时，都需要**重新 source 对应的 env-init 脚本并删除旧 build 目录**，否则 CMakeCache 会残留上次的配置：

```bash
rm -rf build
source cross-profiles/ql-ol-crosstool-env-init-ec200a   # 或 eg25g
mkdir build && cd build
cmake .. ...
```

---

## 已知问题及解决方法

### EG25G：OpenSSL 编译失败（perl 路径冲突）

**现象**：source EG25G 环境后，`perl` 命令解析到了 SDK 自带的 perl，导致 OpenSSL 编译报错。

**解决方法**（三选一）：

```bash
# 方法一：强制 /usr/bin 优先
export PATH=/usr/bin:$PATH

# 方法二：重命名 SDK 里的 perl（一次性）
mv ${QL_SDK_NATIVE_SYSROOT}/usr/bin/perl ${QL_SDK_NATIVE_SYSROOT}/usr/bin/perl.bak

# 方法三：升级 CMake 到 3.27+（根本解决）
```

### EG25G：libev 编译失败（automake 正则异常）

**现象**：libev 构建时 automake 报 "line 3936" 的正则表达式错误。

**解决方法**：编辑 SDK 里的 automake 文件，找到第 3936 行，将有问题的正则表达式替换为合法写法（具体内容视 SDK 版本而定）。

### mosquitto：找不到 cjson 库

**现象**：mosquitto 编译时报找不到 JSON 库。

**解决方法**：在 cmake 配置命令中加入 `-DWITH_CJSON=OFF`：

```bash
# 修改 3rdparty/mosquitto/build.sh，在 cmake 命令末尾加上
-DWITH_CJSON=OFF
```

---

## 可选编译开关

在 `cmake ..` 命令中按需追加：

| 开关 | 说明 |
|------|------|
| `-DENABLE_DISPATCH_OPTIMIZE=ON` | 拨号性能优化（默认已开启） |
| `-DENABLE_SPI_DUMMY=ON` | 虚拟 SPI 数据，无硬件时用于测试 |
| `-DWITH_DATA_RECORDER=1` | 数据记录仪应用 |
| `-DWITH_DIMA_CLIENT=1` | 迪马上数客户端 |
| `-DWITH_RTMS_EXAMPLE=1` | 示例应用 |
