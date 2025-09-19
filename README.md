# 基于单片机的车灯自动控制系统（STM32F103C8T6）

本项目为一套基于 STM32F103C8T6 的“车灯自动控制系统”运行程序，集成光照检测（LDR）、温湿度（DHT11）、超声测距（HC‑SR04）、车速码盘计数、PWM 车灯输出与 OLED 显示，并提供按键交互与参数配置（阈值可存储于片内 Flash）。

---

## 一、项目概览（What & Why）
- **目标**：在自动/半自动场景下，根据环境与行驶状态智能控制近光、远光与雾灯，提高行车安全与体验。
- **核心能力**：多传感器融合（光照/速度/距离/湿度）、分级灯光策略、平滑 PWM 输出、易用的 OLED UI 与可持久化参数配置。
- **硬件平台**：STM32F103C8T6（72MHz，StdPeriph 库）。

---

## 二、主要功能（Features）
- **模式管理**：自动 / 手动 / 配置（长按进入配置，参数保存至 Flash）。
- **智能控制**：
  - 近光：基于光照/速度/距离/隧道检测按等级输出。
  - 远光：在“足够暗、足够快、距离安全”时启用并分级。
  - 雾灯：基于湿度阈值自动开/关。
- **显示与交互**：
  - OLED：速度、光照、距离、温/湿度与模式标识；行级缓冲防闪烁与单位丢失。
  - 按键：短按/长按/连续按，TIM3 1ms 扫描，响应更快。
  - 状态 LED：不同模式不同闪烁频率。
- **持久化**：阈值参数保存于 `0x0800F800`，掉电保持。

---

## 三、硬件连接（默认引脚）
- LED 指示：`PA1`(LED1)、`PA2`(LED2)
- LDR 光敏：`PA7`（ADC Channel 7）
- 超声 HC‑SR04：TRIG `PA8`，ECHO `PA9`（TIM4 计时）
- DHT11：`PB12`
- 码盘/计数：`PA5`（EXTI Line5，下降沿）
- PWM 车灯输出（TIM2）：
  - CH2 → 远光灯：`PA1`
  - CH3 → 近光灯：`PA2`
  - CH4 → 雾灯：`PA3`
  - 注：CH1 `PA0` 也已初始化，可按需使用。
- OLED：见 `Hardware/OLED.*` 实际接线实现。

如需变更引脚，请同步修改对应模块头文件中的宏定义及定时器配置。

---

## 四、目录结构与职责（Folder Structure & Responsibility）
- `User/`
  - `main.c`：系统入口与主循环；初始化所有外设与模块；周期采样（传感器/速度）；调用 `LightControl_Update`；OLED 刷新与系统 LED 指示；防闪烁显示缓冲。
  - `stm32f10x_conf.h`：库使能配置。
  - `stm32f10x_it.*`：中断向量与处理（结合 `System/` 与各硬件模块）。
- `Hardware/`
  - `LightControl.*`：灯光控制核心（模式状态机；近光/远光/雾灯策略；指数平滑 PWM；隧道检测；与按键模块联动）。
  - `Config.*`：参数配置 UI（OLED 列表/单位显示/高亮/闪烁）；参数临时值与提交；Flash 读写（`0x0800F800`）。
  - `LDR.*`：光照 ADC 采样、均值滤波、Lux 与 0~100% 映射。
  - `dht11.*`：DHT11 温湿度采集（GPIO 模式切换、时序读取、校验）。
  - `ultrasonic.*`：HC‑SR04 测距（TRIG 触发、ECHO 计时、超时保护，基于 TIM4）。
  - `PWM.*`：TIM2 PWM 初始化（PA0~PA3）；占空比设置接口（CH1~CH4）。
  - `CountSensor.*`：码盘脉冲计数（PA5 EXTI，消抖，计数与时间戳）。
  - `LED.*`：LED1/LED2 初始化与开关/翻转。
  - `OLED.*`/`OLED_Font.h`：OLED 驱动与字库。
  - `Key.*`（如有）/`KeyEXTI` 由 `System/` 提供增强版。
  - `Config.h` 等：各模块的对外 API。
- `System/`
  - `Delay.*`：系统延时与节拍初始化与接口。
  - `KeyEXTI.*`：TIM3 1ms 扫描的按键输入模块（短按、长按、连发；边沿检测；更小消抖时间）。
  - `timer.*`：通用定时器封装（供超声等计时使用）。
  - `sys.*`：系统级别的基础封装（时钟/宏等，视实现）。
- `Library/`
  - ST 标准外设库（GPIO/TIM/USART/I2C/ADC/EXTI/RCC 等驱动源码与头文件）。
- `Start/`
  - 启动与系统文件：`startup_stm32f10x_*.s`、`system_stm32f10x.*`、`core_cm3.*`、`stm32f10x.h`。
- `DebugConfig/`
  - 调试目标配置（例如 Keil 的目标调试设置 `.dbgconf`）。
- `Objects/`、`Listings/`
  - 构建产物目录（目标文件、中间文件与列表输出）。
- 工程文件
  - `Project.uvprojx`、`Project.uvoptx`、`Project.uvguix.*`：Keil uVision 工程与用户配置。
  - `keilkill.bat`：构建/清理辅助脚本（按需）。

---

## 五、各文件夹需要做的内容（Responsibilities & Typical Tasks）
- `User/`
  - 集成应用层逻辑、主循环节拍与任务编排。
  - 调用各硬件/系统模块 API，组织数据采样、控制与显示。
  - 新增业务功能、页面/显示刷新策略与状态指示。
- `Hardware/`
  - 开发/适配具体传感器与执行器驱动（LDR、DHT11、HC-SR04、PWM、OLED、LED、码盘等）。
  - 实现控制策略（`LightControl.*`）与参数配置（`Config.*`）。
  - 封装稳定的对外接口头文件（含引脚与参数宏）。
- `System/`
  - 提供与芯片强相关的基础能力（延时、按键扫描、通用定时器等）。
  - 保证 1ms 节拍、去抖、长按/连发等基础交互可靠性。
- `Library/`
  - 维护 ST 官方外设库版本与最小必要子集，避免随意修改以减小维护成本。
- `Start/`
  - 维护启动文件与系统时钟、向量表；如需迁移/改频率在此修改。
- `DebugConfig/`
  - 管理调试目标与下载设置（电压、复位、擦写策略等）。
- `Objects/`、`Listings/`
  - 仅作构建输出保存；无需手改，清理构建可直接删除后重建。
- 工程文件（`*.uvprojx` 等）
  - 管理工程配置（包含路径、宏定义、优化等级、下载器设置）。

---

## 六、项目目录树（含说明）
```text
基于单片机的车灯自动控制系统 运行程序/
├─ DebugConfig/               # 调试目标配置（Keil 调试器参数等）
├─ Hardware/                  # 具体硬件驱动与控制逻辑
│  ├─ Config.*                # 参数配置界面/UI、Flash 存取
│  ├─ CountSensor.*           # 码盘计数（PA5 EXTI）
│  ├─ dht11.*                 # DHT11 温湿度
│  ├─ LDR.*                   # 光敏 LDR 采样与百分比
│  ├─ LED.*                   # LED1/LED2 指示灯
│  ├─ LightControl.*          # 灯光控制核心策略与 PWM 输出
│  ├─ OLED.* / OLED_Font.h    # OLED 驱动与字库
│  ├─ PWM.*                   # TIM2 PWM 初始化与占空比设置
│  ├─ ultrasonic.*            # HC‑SR04 超声测距（TIM4 计时）
│  └─ ...                     # 其他硬件相关文件
├─ Library/                   # STM32F10x 标准外设库源码与头文件
├─ Listings/                  # 构建列表输出（编译器/链接器日志等）
├─ Objects/                   # 构建中间产物/目标文件
├─ Start/                     # 启动与系统文件
│  ├─ startup_stm32f10x_*.s   # 启动文件与中断向量
│  ├─ system_stm32f10x.*      # 系统时钟等配置
│  └─ core_cm3.* / stm32f10x.h# CMSIS 与芯片头文件
├─ System/                    # 基础系统能力与通用外设封装
│  ├─ Delay.*                 # 延时/节拍
│  ├─ KeyEXTI.*               # 按键扫描（TIM3 1ms）
│  ├─ timer.*                 # 通用定时器封装
│  └─ sys.*                   # 系统级封装（如适用）
├─ User/                      # 应用层入口与中断
│  ├─ main.c                  # 主循环、初始化、调度与显示
│  ├─ stm32f10x_conf.h        # 库配置
│  └─ stm32f10x_it.*          # 中断实现
├─ Project.uvprojx            # Keil uVision 工程
├─ Project.uvoptx             # Keil 用户/调试配置
├─ Project.uvguix.*           # Keil GUI 相关配置
├─ keilkill.bat               # 构建/清理辅助脚本
└─ README.md                  # 项目说明文档
```

---

## 七、可配置参数（Config）
- `PARAM_LUX`（默认 60）：光照阈值，环境亮度超过阈值则降级或关闭灯光。
- `PARAM_SPD`（默认 40 cm/s）：速度阈值，高速适度提高灯光等级。
- `PARAM_DIS`（默认 50 cm）：距离阈值，过近抑制远光，避免炫目。
- `PARAM_HUM`（默认 85%）：湿度阈值，高湿时开启雾灯。

范围 0~100。配置界面带单位（lx、cm/s、cm、%），保存 Flash 后掉电不丢失。

---

## 八、构建与下载（Build & Flash）
- 工具链：Keil uVision + STM32F10x StdPeriph 库。
- 打开工程：`Project.uvprojx`。
- 目标芯片：STM32F103C8T6（72MHz）。
- 下载：通过 ST‑Link 等调试器进行烧录。
- 常见检查：
  - 库路径/包含路径是否正确；
  - 启动文件与系统时钟配置是否匹配（`Start/` 与 `system_stm32f10x.*`）；
  - 供电与外设接线是否与默认引脚一致。

---

## 九、运行与显示（Runtime）
- 上电自检：OLED 显示 “System Ready”，随后进入主界面。
- 主界面：
  - 第1行：Spd: xxx cm/s + 模式标识(A/M)
  - 第2行：Lux: xx %
  - 第3行：Dst: xx.x cm
  - 第4行：T:xx C  H:xx %
- LED1 按模式以不同频率闪烁。

---

## 十、关键实现要点（Engineering Notes）
- **速度计算**：码盘脉冲差与时间差换算 cm/s；`ZERO_SPEED_TIMEOUT_MS=200` 超时判 0。
- **显示防闪烁**：行级缓存与定长覆盖，确保单位不丢失；模式切换强制重绘。
- **PWM 平滑**：指数平滑逼近目标占空比，消除亮度跳变。
- **隧道检测**：短时间光照突降置 `tunnelFlag`，近光提升到高等级；定时自动清除。
- **配置超时**：配置模式支持超时自动保存并提示。

---

## 十一、维护与扩展（Maintenance & Extension）
- 变更引脚：修改对应 `Hardware/*.h` 宏与 `System/timer.*` 配置。
- 增加传感器：在 `Hardware/` 新增驱动与接口，并在 `main.c`/`LightControl.c` 融合逻辑。
- 调整策略：修改 `LightControl.c` 中的阈值计算与等级映射，保持单位一致性。

---

## 十二、许可与鸣谢（License & Credits）
- 依赖 STM32 标准外设库（见 `Library/`）。
- 部分传感器驱动参考常见开源实现并做适配与增强。

如需移植至其他平台或板卡，请据此调整宏定义、时钟、定时器与引脚配置。
嗯