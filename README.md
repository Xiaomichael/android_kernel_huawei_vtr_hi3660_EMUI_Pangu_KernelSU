# Pangu_Kernel 4.9.200 示例内核
## 前情提要：
> [!NOTE]
> 如需使用补丁请将本仓库拉取到本地后cherry-pick相对应的commit id即可合并更新！

> [!WARNING]
> **技术预览版本**\
> 此版本内核正处于内核升级开发-[技术预览版本]\
> 同时作为自定义内核，与官方内核相比有许多不稳定因素！\
> 相关功能正在尝试添加，问题正在尝试修复

> [!NOTE]
> 此版本内核不参与自动构建，仅供内核实例展示，含有相关git记录\
> 此开发分支在 [9.0-200_develop](https://github.com/yunmo2007/android_kernel_huawei_vtr_hi3660_EMUI_Pangu_KernelSU/tree/9.0-200_develop) 中

***
## 内核所支持的机型：
P10版：P10，P10Plus \
V9版：荣耀9，8Pro（V9），Nova2S，平板M5(krin960)，Mate9（Pro），Nova2（初步支持）
> [!WARNING]
> 此版本的内核适用于 EMUI9.0/GSI \
> 不同版本之间的设备驱动版本不同，请不要混刷，否则会出现黑屏和功能无法使用！\
> 详细的说明(强烈建议先去阅读此文件！！)：[旧版README](README_OLD.md)

*** 
## Release版本说明：
分为两个压缩包，内部为历史版本构建（只构建了V9版本，P10版本需要自行构建），如有需要的可以自行下载测试
 > 解压后带enforcing的版本刷入后开机SELinux为强制(严格)模式。带permissive的版本刷入后开机SELinux为宽容模式。

***
## 此版本内核存在的问题：
 + 在使用[adb]连接设备时[/]目录下的“ls”命令中出现部分SELinux权限问题\
 缓解方案：添加SELiunx放行规则（这里使用magiskpolicy）\
 magiskpolicy --live \ \
  "allow shell rootfs file { getattr open read }" \ \
  "allow shell rootfs dir { getattr search read }" \ \
  "allow shell modem_log_file dir { getattr search read }" \ \
  "allow shell teecd_data_file dir { getattr search read }" \ \
  "allow shell init_exec file getattr"
 + 使用Apatch修补版本后只能使用superuser相关，无法挂载模块
 + 以及其他未发现的问题...


# 额外文档
+ 一些过往忽略的补丁
1. 补丁忽略说明[文件](https://github.com/yunmo2007/android_kernel_huawei_vtr_hi3660_EMUI_Pangu_KernelSU/blob/9.0-200/Ignored_patches.md)
+ 感谢原作者提供的思路（这两篇并不是我写的但是还是表示感谢）
1. 关于刷机的一些教程:[Wiki](https://github.com/Coconutat/HuaweiP10-GSI-And-Modify-Tutorial/wiki)  
2. 适配华为EMUI9/9.1.0内核的教程:[Wiki](https://github.com/Coconutat/HuaweiP10-GSI-And-Modify-Or-Support-KernelSU-Tutorial/wiki/7.KernelSU%E9%80%82%E9%85%8DEMUI9%E6%88%969.1.0%E7%B3%BB%E7%BB%9F%E7%9A%84%E5%86%85%E6%A0%B8)  

***  
# 创建者/贡献者（排名不分先后）： 
## 原始项目
 + [麦麦观饭](https://github.com/maimaiguanfan) / [麒麟盘古内核](https://github.com/maimaiguanfan/android_kernel_huawei_hi3660/)：提供了内核参考以及基础的内核。
 + [kindle4jerry](https://github.com/kindle4jerry) : 感谢大佬的建议和无私帮助。  
 + [aaron74xda](https://github.com/aaron74xda) / [android_kernel_huawei_hi3660
](https://github.com/aaron74xda/android_kernel_huawei_hi3660):启发了我对于华为内核的强制SElinux宽容的具体思路。
 + [OnlyTomInSecond](https://github.com/OnlyTomInSecond) / [android_kernel_xiaomi_sdm845](https://github.com/OnlyTomInSecond/android_kernel_xiaomi_sdm845):提供了KernelSU的移植思路。  
 + [Aquarius223](https://github.com/Aquarius223) / [android_kernel_xiaomi_msm8998-ksu](https://github.com/sticpaper/android_kernel_xiaomi_msm8998-ksu)：修改SElinux的hook.c实现模块功能(可能吧)。  
 + [术哥](https://github.com/tiann) / [KernelSU](https://github.com/tiann)：开发了牛逼闪闪的各种炫酷东东的大佬。没有他就没有KernelSU。感谢他在我折腾华为内核期间给予的帮助。
 + [lateautumn233](https://github.com/lateautumn233) / [android_kernel_oneplus_sm8250](https://github.com/lateautumn233/android_kernel_oneplus_sm8250)：启发我使用Github Action编译内核。(解决了我外地上班只有手机的痛点。)
## 现在项目
 + [Linux上游-4.9分支](https://github.com/gregkh/linux/tree/linux-4.9.y)：提供了内核upstream补丁；我在此基础上，添加了对Huawei功能的修复，确保了最基础的开机。
 + [Coconutat](https://github.com/Coconutat) / [带有原版KSU的内核(9.0)](https://github.com/Coconutat/android_kernel_huawei_vtr_emui9_KernelSU) [带有原版KSU的内核(9.1.0)](https://github.com/Coconutat/android_kernel_huawei_hi3660_emui9.1.0_KernelSU):提供了基础的KSU编译思路与此MD文件的基本格式。
 + [KernelSU_Next](https://github.com/KernelSU-Next) / [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next):解决了KernelSU在5系内核之前的支持，并且使项目转移到KernelSU_Next。
 + [SukiSU-Ultra](https://github.com/SukiSU-Ultra) / [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra):解决了KernelSU在5系内核之前的支持，并且使项目转移到SukiSU-Ultra。
 + [rsuntk](https://github.com/rsuntk) / [KernelSU-legacy](https://github.com/rsuntk/KernelSU):解决了KernelSU在5系内核之前的支持，并且使项目转移到KernelSU-legacy。
 + [ReSukiSU](https://github.com/ReSukiSU) / [ReSukiSU](https://github.com/ReSukiSU/ReSukiSU):解决了KernelSU在5系内核之前的支持，并且使项目转移到ReSukiSU。
 + [simonpunk](https://gitlab.com/simonpunk) / [Susfs](https://gitlab.com/simonpunk/susfs4ksu):新隐藏方式的作者（结合Susfs花费了很久的时间）。

***
#### 滑稽  
![alt 术哥评价适配华为内核行为](https://s1.ax1x.com/2023/03/29/ppgmvo4.png)
