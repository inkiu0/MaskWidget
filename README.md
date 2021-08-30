<h1 align="center">- UE4MaskWidget -</h1>

<h4 align="center">UE4自定义遮罩，提供区域镂空和点击穿透，特别适合新手引导</h4>

<p align="center">
<img src="https://img.shields.io/badge/version-2021.08.26-green.svg?longCache=true&style=for-the-badge">
<img src="https://img.shields.io/appveyor/build/gruntjs/grunt?style=for-the-badge">
<img src="https://img.shields.io/badge/license-SATA-blue.svg?longCache=true&style=for-the-badge">
</p>

## 部署指南

SlateClickClippingState.h   放到 Engine\Source\Runtime\SlateCore\Public\Layout

SlateClickClippingState.cpp 放到 Engine\Source\Runtime\SlateCore\Private\Layout

使用手册请参考以下文章：
1. [【UEInside】编写自定义控件——镂空遮罩Mask（一）](https://zhuanlan.zhihu.com/p/353874773)
2. [【UEInside】编写自定义控件——镂空遮罩Mask（二）](https://zhuanlan.zhihu.com/p/354708184)
3. [【UEInside】编写自定义控件——镂空遮罩Mask（三）](https://zhuanlan.zhihu.com/p/354793040)

## 注意事项
MaskTexture必须是RGBA32，ETC和ASTC都是基于块压缩的，压缩后取到的值是不对的。

目前一个控件上只支持最多3个Clip，如果需要增加Clip，需要修改shader中的clip数量，然后修改cpp中的MAX_MASK_CLIP_COUNT值。

## 许可证

MaskWidget is under The Star And Thank Author License (SATA)

本项目基于MIT协议发布，并增加了SATA协议

您有义务为此开源项目点赞∠( ᐛ 」∠)＿
