# 智能语音小夜灯

基于启英泰伦CI1122芯片开发, 目前支持红外遥控夜灯(NEC协议), 未来将支持直接使用PWM控制

## 构建

首先到启英泰伦开发者后台下载CI112X的SDK(本项目使用V1.4.4版本), 然后将此仓库克隆到CI112X_SDK/sample/internal目录下

接下来需要应用一下红外组件的补丁, 将仓库的patch/ir_remote_driver目录下的两个文件覆盖到CI112X_SDK/components/ir_remote_driver即可

最后用官方提供的eclipse导入此仓库即可, 具体的构建和固件打包流程可参考官方教程

夜灯目前分为PWM控制和红外控制两种, 可以通过light.h中的宏LIGHT_TYPE来切换

如果是红外控制则目前仅支持NEC协议且无自学习功能, 需要自行想办法读出遥控器的按键码, 我用的是ESP8266+Arduino+IRremoteESP8266

## 电路

为了方便, 本项目直接使用了官方商城售卖的CI-C22GS02S模组, 你也可以自己画板子

若采用红外模式, 则红外发射管的正极接在模组的PWM3上

## 版权声明

patch/ir_remote_driver来自启英泰伦CI112X_IR_SDKV1.1.5_V4, 版权归启英泰伦公司所有

其余代码和原理图采用MIT协议开源
