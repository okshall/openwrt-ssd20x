
# openwrt-sstar
wireless-tag 支持sigmstar SSD201/SSD202
支持16M nor flash
# 安装依赖
ubuntu 16.04.7 64位系统

````sh
sudo apt-get install subversion build-essential libncurses5-dev zlib1g-dev gawk git ccache \
		gettext libssl-dev xsltproc libxml-parser-perl \
		gengetopt default-jre-headless ocaml-nox sharutils texinfo
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install zlib1g:i386 libstdc++6:i386 libc6:i386 libc6-dev-i386
````

# 下载代码
1. 下载主工程代码
```
git clone https://github.com/wireless-tag-com/openwrt-ssd20x.git
```

2.  解压缩toolchain

```
下载toolchain 链接：https://pan.baidu.com/s/1SUk1a-drbWo1tkHQzCgchg 提取码：1o3d
mkdir -p openwrt-ssd20x/toolchain
tar wt-gcc-arm-8.2-2018.08-x86_64-arm-linux-gnueabihf.tag.gz -xvf -C openwrt-ssd20x/toolchain
```

# 编译

1. 生成机型配置文件

```
cd 18.06
./scripts/feeds update -a
./scripts/feeds install -a -f
make WTNOR_wt
```

2. 编译

```
make V=s -j4
```

3. 编译产物
    位于bin/target/sstar/ssd20x/WTNOR

| 文件名                   | 说明                           |
| ------------------------ | -------------------------------|
| WTNOR-sysupgrade.bin     | 网页升级kernel+rootfs          |
| WTNOR-all.bin			   | 量产烧写ipl+uboot+kernel+rootfs|
| WTNOR-boot.bin		   | ipl+uboot                      |


4. 升级

4.1. 通过网页升级
登录网页->系统->备份/升级

4.2. uboot 升级
进入调试串口，开机长按回车键
```
setenv gatewayip 192.168.1.1
setenv serverip 192.168.1.88
setenv ipaddr 192.168.1.11
setenv netmask 255.255.255.0
saveenv
run sysupgrade
reset
```
4.3. 串口升级
进入调试串口，开机长按回车键,输入debug,关闭串口软件,打开Flash_Tool_5.0.16
烧写WTNOR-all.bin 或 WTNOR-boot.bin

4.4 系统下升级
```
cd /tmp
tftp -g 192.168.1.88 -r WTNOR-sysupgrade.bin
sysupgrade WTNOR-sysupgrade.bin
```
升级完成之后，系统将自动重启

5. 分区配置
```
----------------------0x0
|IPL.bin      0x10000|
----------------------0x10000
|IPL_CUST.bin 0x10000|
----------------------0x20000
|MXP_SF.bin	  0x10000|
----------------------0x30000
|uboot.bin    0x3E000|
----------------------
|UBOOT_ENV    0x2000 |
----------------------0x70000
|kernel      0x290000|
----------------------0x300000
|rootfs      0xcf0000|
----------------------0xff0000
|factory      0x10000|
----------------------
```
MXP_SF.bin 分区信息生成脚本
18.06/package/sigmastar/uboot-sstar/noripl/mkparts.sh

sysupgrade，all 固件生成脚本
18.06/target/linux/sstar/image/Makefile


