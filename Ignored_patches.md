# 额外文档-忽略补丁列表
## 说明：
+ 此页面是用于存放在从[Linux上游-4.9分支](https://github.com/gregkh/linux/tree/linux-4.9.y)中所忽略的补丁列表，其中未应用的多为卡第一屏或二屏的补丁\
### 列表：
4.9.142 忽略UFS_SCSI补丁\
git cherry-pick 675d91966d1ab9ea0371c66b126d1427aaac0415^..f8c66aba27fa9aef4aa0560bea3e6ba48a546f63\
忽略int ufshcd_scale_clks，ufshcd_devfreq_target，ufshcd_devfreq_get_dev_status，恢复：ufshcd_remove\
4.9.144 忽略F2FS部分补丁\
git cherry-pick 3b19f961d260d1ae12b497e5eb77e5ecc1039fac^..91fe514bedf4c72ae8046fe4cfa98c5e201f6b84\
4.9.165 忽略PM/wakeup补丁\
git cherry-pick 6f76eeca250309dd2168878fbf3e3375ab18eb69\
4.9.166 忽略scsi补丁\
git cherry-pick 321c5ade5a06cb561459b5190ed5bd36a270ea5e\
4.9.190 忽略asm-generic 补丁\
git cherry-pick 912420e5252c6867d3c3c3ad7a69ca59e5fc48c8\
4.9.195 忽略f2fs部分补丁\
git cherry-pick 002d26801873709c7a07faa7b992c8d725d300ac^..f3537bd135ebdcd194efdc5a490d299eb1318870\
忽略ANDROID部分补丁（无法开机）\
git cherry-pick a494a71146a1cf3f48bb94cf33981db1f027e6a0^..b6c6212514fe9f2387fc6677181028d4a9ae20c7