**简体中文** · [English](CI-build-and-release.md)

# 构建与发布

`.github/workflows/build-firmware.yml` 在推送标签或手动触发时构建固件。
它恢复 ccache，使用 ESP-IDF 5.5.3 为 ESP32-C3 运行 `./tools/validate.sh --firmware`，
验证镜像布局和 8 MB Flash 配置，并将 `FoloToy-AI-Passport-full.bin` 上传为 Actions artifact。
普通分支推送使用单独的静态检查和固件检查工作流。

所有 Action 固定到完整 commit SHA，构建工作流只有 `contents: read` 权限。
它不会创建或更新 GitHub Release：发布固件由负责人从实机验收的原始文件中明确选取。
CI 重建可能包含不同的构建元数据，不得悄悄替换已验收固件。

## 发布准备

1. 校验已验收固件的归档，保留完整镜像 SHA-256。
2. 同步更新[更新记录](../../CHANGELOG.zh_CN.md)的中英文版本，只记录当前玩法已验证的用户可见变化。保留新的 `Unreleased` 节，不把沿用的上游说明当成本玩法的发布历史。
3. 提交匹配的源码和文档。标签带上应用名，例如 `v1.0.0-guandan`，标题例如 `v1.0.0 Guandan Scoreboard`。
4. 发布原始完整镜像、SHA-256 校验和与干净的安装包；分段刷写使用已验证的对应组件镜像。排除凭据、个人路径、设备原始日志和本地调试清单。
5. 编写中英文 Release 说明，覆盖功能、操作、安装、对保存数据的影响、构建命令与实际测试范围。发布后核对标题和下载文件哈希。

## 安装与保存数据

[官方网页刷机工具](https://ai-passport.folotoy.cn/tools/web-flasher/)将完整的
`FoloToy-AI-Passport-full.bin` 从 `0x0` 写入，并替换当前应用。
完整镜像的填充部分跨过存储区，可能清空已保存设置。需要保留数据时，先核对设备真实分区布局，
再按安装包的分段刷写说明操作。应用单独镜像不能写到 `0x0`；本项目应用地址为 `0x10000`。
无需整片擦除。

详见[固件布局](../engineering/firmware-layout.zh_CN.md)和
[掼蛋应用说明](../../applications/guandan.zh_CN.md)。

## 重新构建

激活 ESP-IDF 5.5.3 后，运行 `./tools/validate.sh` 完成全部检查，或运行
`./tools/validate.sh --firmware` 完成构建、合并、校验和归档。
单独运行 `idf.py build` 不会产出经过完整检查的合并交付镜像。
构建成功不代表重新生成的固件已经过实机测试。

社区提交与 GitHub Release 分开进行，详见
[publish-to-community.zh_CN.md](../release/publish-to-community.zh_CN.md)。
