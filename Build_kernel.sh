#!/bin/bash
#设置环境

# Special Clean For Huawei Kernel.
if [ -d include/config ];
then
    echo "Find config,will remove it"
	rm -rf include/config
else
	echo "No Config,good."
fi


echo " "
echo "***Setting environment...***"
# 交叉编译器路径
export PATH=$PATH:$(pwd)/../ToolChains/bin
export CROSS_COMPILE=aarch64-linux-gnu-
export GCC_COLORS=auto
export ARCH=arm64

start_time=$(date +%Y.%m.%d-%I_%M)

start_time_sum=$(date +%s)

#构建V9内核部分
echo "***Building for V9 version...***"
make ARCH=arm64 O=out Pangu_V9_defconfig
# 定义编译线程数
make ARCH=arm64 O=out -j8

end_time_sum=$(date +%s)

end_time=$(date +%Y.%m.%d-%I_%M)

# 计算运行时间（秒）
duration=$((end_time_sum - start_time_sum))

# 将秒数转化为 "小时:分钟:秒" 形式输出
hours=$((duration / 3600))
minutes=$(( (duration % 3600) / 60 ))
seconds=$((duration % 60))

# 打印运行时间
echo "脚本运行时间为：${hours}小时 ${minutes}分钟 ${seconds}秒"

#打包V9版内核

if [ -f out/arch/arm64/boot/Image.gz ];
then
	echo "***Packing V9 version kernel...***"

	# Pack Enforcing Kernel
	tools/mkbootimg --kernel out/arch/arm64/boot/Image.gz --base 0x0 --cmdline "loglevel=4 initcall_debug=n page_tracker=on slub_min_objects=16 unmovable_isolate1=2:192M,3:224M,4:256M printktimer=0xfff0a000,0x534,0x538 androidboot.selinux=enforcing buildvariant=user" --tags_offset 0x07A00000 --kernel_offset 0x00080000 --ramdisk_offset 0x07c00000 --header_version 1 --os_version 9 --os_patch_level 2020-09-05  --output Pangu_V9-enforcing-DEV.img
	
	# Pack Permissive Kernel
	tools/mkbootimg --kernel out/arch/arm64/boot/Image.gz --base 0x0 --cmdline "loglevel=4 initcall_debug=n page_tracker=on slub_min_objects=16 unmovable_isolate1=2:192M,3:224M,4:256M printktimer=0xfff0a000,0x534,0x538 androidboot.selinux=permissive buildvariant=user" --tags_offset 0x07A00000 --kernel_offset 0x00080000 --ramdisk_offset 0x07c00000 --header_version 1 --os_version 9 --os_patch_level 2020-09-05  --output Pangu_V9-permissive-DEV.img

	make clean
	
	echo "***Sucessfully built P10 version kernel...***"
	echo " "
	exit 0
else
	echo " "
	echo "***Failed!***"
	exit 0
fi