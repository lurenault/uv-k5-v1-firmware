# UV-K5（k5-v6 / bg7nzl）使用手册（中文）

面向电台使用者。说明本固件相对 egzumer 系基线多出来的功能、怎么进入、菜单在哪、默认值与注意点。

英文版：[`docs/USER_MANUAL_BG7NZL.md`](USER_MANUAL_BG7NZL.md)

---

## 对照基线与版本说明

| 项 | 值 |
|----|-----|
| 本手册对应固件树 | `k5-v6` 当前 HEAD |
| 对照基线（原版 egzumer 系） | `a6eda00ab1f2915d0da49b5aa65ce3adaa0532df`（2024-03-26，`Fix typo in README.md`） |
| 手册撰写时 HEAD | `a70a9fbefa2440ee513ab1c319822d9eb30a688a`（`fix(roger): key Morse with TxMute; use 750 Hz Tone1`） |
| 适用机型 | 泉盛 UV-K5 / K6 等 DP32G030 + BK4819 机型（本树打包目标） |

基线已在本地用 `git cat-file -t` 确认为有效 commit。下文「相对原版」均相对该 SHA。

**合法使用提醒：** 请遵守当地业余无线电法规（频率、功率、标识、中继/APRS 许可等）。本手册只描述固件操作，不构成任何发射授权。

---

## 1. 简介与相对原版差异总览

本固件在 egzumer 自定义固件基础上，默认打开一批面向数字通信与实用增强的功能，并关掉部分占空间的功能。

### 1.1 新增 / 增强（用户可见）

| 功能 | 一句话 |
|------|--------|
| **Beacon** | 菜单开关：手电 LED 按航空障碍灯节奏慢闪（约每 5 秒闪一下） |
| **APRS 收发 + Digipeater** | 侧键进屏；RF 用**当前选中 VFO** 频率+功率；临时 FM/宽带/关亚音/关压扩，退出恢复；Modem 源自 ta1js；可显示呼号/网格/注释；DgPeat：OFF / n-N / echo |
| **MyCall + Morse Roger** | 菜单 MyCall（≤6，EEPROM `0x0E30`，与 DgCall 独立）；Roger=MORSE 时 FM 松 PTT 以 **Tone1 + TxMute** 发约 **750 Hz** 音频模拟 CW（非真 CW）；空呼号静默 |
| **CW 调制** | `Demodu` 增加 CW；CW 发射为载波 + 本机 650 Hz 侧音 |
| **AM / USB 话音发射** | 可在 AM、USB 模式下按 PTT 发射（原版默认常禁非 FM 发射） |
| **Digimode（UART 数字模式）** | 电脑通过编程口精确控频发射（FT8 等）；电台显示 DIG 界面 |
| **CAT 遥控** | 电脑通过编程口读写当前 VFO；状态栏显示 `CAT` |
| **精简广播 FM** | 仅 `F+0` 进入；64–108 MHz；上下键步进；无信道记忆 / 无扫描 |

### 1.2 默认关闭或行为变化（相对基线）

| 项 | 基线默认 | 本固件默认 | 用户感受 |
|----|----------|------------|----------|
| **DTMF** | 开启（含呼叫相关） | **关闭**（`ENABLE_DTMF=0`） | 无 DTMF 菜单、无 DTMF 输入相关行为 |
| **频谱仪 Spectrum** | 开启 | **关闭**（`ENABLE_SPECTRUM=0`） | 无 `F+5` 频谱、侧键列表无 SPECTRUM |
| **侧键进 FM** | 侧键可绑 FM RADIO | **侧键绑 FM 无效** | 广播 FM 只能 `F+0` |
| **下侧键长按** | 常见为无动作等 | 启用 APRS 时默认 **APRS** | 长按下侧键进 APRS 屏 |
| **Roger** | OFF / ROGER / MDC | 增加 **MORSE** | 需配合 MyCall |

### 1.3 编译开关一览（默认构建）

下列为 `Makefile` 默认值，反映「出厂」固件能力（自行改编译另论）：

| 开关 | 默认 | 含义 |
|------|------|------|
| `ENABLE_UART` | 1 | 编程串口 / Digimode / CAT 依赖 |
| `ENABLE_DIGMODE` | 1 | Digimode |
| `ENABLE_CATMODE` | 1 | CAT |
| `ENABLE_APRS` | 1 | APRS |
| `ENABLE_FLASHLIGHT` | 1 | 手电 + Beacon 菜单 |
| `ENABLE_FMRADIO` | 1 | 精简 FM |
| `ENABLE_DTMF` | **0** | 总 DTMF 关 |
| `ENABLE_DTMF_CALLING` | **0** | DTMF 呼叫关 |
| `ENABLE_SPECTRUM` | **0** | 频谱关 |

---

## 2. 刷机与多固件注意（简要）

1. **刷机文件**  
   请使用发布包中的 **packed** 固件（如 `*.packed.bin` / 发布说明中的 `bg7nzl-k5v1-<短SHA>.bin`），用常规 UV-K5 刷机工具写入。未打包的裸 `.bin` 可能不被部分工具接受。

2. **EEPROM / 信道**  
   本固件在原有配置区「空洞」中增加了 MyCall、APRS Digipeater、Beacon 等位。一般可保留信道与大部分设置；若菜单出现异常值，用菜单 **Reset → VFO** 或 **ALL**（会清空更多设置，慎用）。

3. **换回原版 / 其它固件**  
   可随时刷回 egzumer 或其它固件。刷回后：MyCall / DgPeat 等新菜单项会消失；其 EEPROM 数据通常无害，但不同固件对空洞用法不同，异常时再 Reset。

4. **编程线与工作冲突**  
   Digimode、CAT、常规 CPS 刷写都占用同一 UART。做 Digimode / CAT 时不要同时开 CPS；做 APRS 实测（尤其测转发时序）时，建议**拔掉编程线 / 关掉占用串口的软件**，避免串口活动影响电台与计时观感（见附录 FAQ）。

5. **波特率**  
   Digimode / CAT 默认按编程口 **38400** 与电脑通信（USB-CDC 适配器按其设备速率）。

---

## 3. 侧键与快捷键（本固件相关）

### 3.1 可配置侧键（菜单）

菜单项：**F1Shrt / F1Long / F2Shrt / F2Long / M Long**  
（上侧键短/长按、下侧键短/长按、MENU 键长按）

本固件侧键可选功能（在默认编译下）包括：

| 显示名 | 作用 |
|--------|------|
| NONE | 无 |
| FLASH LIGHT | 手电：关 → 常亮 → 快闪 → SOS → 关 |
| POWER | 切换功率 |
| MONITOR | 监听（开静噪） |
| SCAN | 扫描 |
| VOX | 开关 VOX |
| LOCK KEYPAD | 键盘锁 |
| SWITCH VFO | A/B |
| VFO/MR | 频率/信道 |
| SWITCH DEMODUL | 切换调制（FM→AM→USB→CW→…） |
| APRS | 进入/退出 APRS 界面 |

说明：

- **FM RADIO 不再出现在侧键列表**；侧键即使 EEPROM 里仍记着旧的 FM 动作，也会变成无操作。
- **SPECTRUM 默认不编译**，列表中无此项；`F+5` 也不会进频谱。
- **启用 APRS 时，下侧键长按（F2Long）出厂默认 = APRS**（EEPROM 未配置或无效时回落为此默认）。

### 3.2 固定快捷键

| 操作 | 作用 |
|------|------|
| `F` + `0` | 进入 / 退出广播 FM |
| 主界面侧键 APRS（或已绑定的侧键） | 进入 / 退出 APRS（用当前 VFO 频率/功率） |
| Digimode / APRS 界面按 `EXIT` | 退出该界面回主屏 |
| 主界面长按侧键切换调制 | 若绑了 SWITCH DEMODUL |

---

## 4. Beacon（障碍灯慢闪）

### 4.1 是什么

在**手电关闭**时，按航空障碍灯类似节奏点亮机身 LED：约 **120 ms 亮、约 5 s 周期**，便于夜间识别电台位置。

### 4.2 如何开关

1. 按 `MENU` 进入菜单。  
2. 找到 **Beacon**（在 Beep 附近）。  
3. 设为 **OFF** 或 **ON**，确认保存。

### 4.3 默认值与注意

| 项 | 值 |
|----|-----|
| 默认 | **OFF**（全新 EEPROM `0xFF` 视为关） |
| 与手电关系 | 手电处于常亮 / 快闪 / SOS 时，Beacon **不抢灯**；关掉手电后才继续慢闪 |
| 侧键 FLASH LIGHT | 仍可手动开关手电模式；Beacon 是独立菜单项 |

---

## 5. Roger 与 MyCall（莫尔斯尾音）

松 PTT 结束 **FM** 话音发射时，若 Roger 设为 **MORSE**，电台用本机 **MyCall** 以莫尔斯发一遍呼号，作为发射结束标识。

实现上是 **音频模拟 CW**（非真 CW 键控载波）：在仍处于 FM 发射结束段时，用 BK4819 **Tone1** 产生约 **750 Hz** 单音，并以 **TxMute** 开/关键键控点划（与传统 Roger 双音同类通路）。空中听起来像短促 CW 呼号，但调制方式仍是 FM 话音链路上的单音，不是 `Demodu=CW` 那种真 CW。

### 5.1 Roger 菜单

菜单：**Roger**（紧挨其后是 **MyCall**）

| 选项 | 行为 |
|------|------|
| OFF | 无尾音 |
| ROGER | 传统 Roger 双音 |
| MDC | MDC 式尾音 |
| **MORSE** | 松 PTT 后以莫尔斯发送 **MyCall** |

触发时机：常规发射结束路径（`RADIO_SendEndOfTransmission`）里调用 Roger。  
**CW / AM / USB** 发射结束会提前返回，**不会**播放任何 Roger（含 MORSE）。因此 Morse Roger **只作用于 FM 话音**松 PTT。

### 5.2 MyCall

菜单：**MyCall**

| 项 | 说明 |
|----|------|
| 用途 | 仅供 Roger=**MORSE** 发射结束莫尔斯；不作他用 |
| 长度 | 最多 **6** 字符 |
| 字符 | `A`–`Z`、`0`–`9`（小写保存时转大写） |
| 空白 | `_` / 空格在保存时去掉；未设置时菜单显示 `-` |
| 空呼号 | MyCall 为空时，即使 Roger=MORSE，松 PTT **静默不发** |
| 存储 | EEPROM **`0x0E30`–`0x0E37`**（8 字节区，前 6 为呼号；与 DigiCall `0x0F20` 独立） |
| 与 DgCall | **完全独立**：DgCall 只给 APRS Digipeater；改 MyCall 不影响 DgCall，反之亦然（编辑 UI 类似，缓冲区不同） |

莫尔斯参数（固件内置）：约 **100 WPM**（点长约 12 ms）、Tone1 **750 Hz**、TxMute 键控。极快，作短标识用，不是练习抄报用的慢速。

### 5.3 推荐设置步骤

1. 菜单 → **MyCall** → 写入你的呼号（如 `BG7NZL`，最多 6 字符）。  
2. 菜单 → **Roger** → **MORSE**。  
3. 切到 **FM**，按 PTT 通话后松手，听尾音是否为你的呼号（约 750 Hz）。  
4. 若不想发尾音：Roger 改回 OFF / ROGER / MDC，或清空 MyCall。

---

## 6. 调制：CW / AM / USB

### 6.1 切换方式

- 菜单 **Demodu**：FM / AM / USB / **CW**（以及编译开启时的其它项）。  
- 或侧键绑 **SWITCH DEMODUL** 循环切换。

### 6.2 CW

| 项 | 说明 |
|----|------|
| 接收 | 按 USB 类解调，并带约 **650 Hz** 音高偏移（听起来像边带 CW） |
| 发射 | 按 PTT 发载波；扬声器有 **650 Hz** 侧音方便自己听节奏 |
| 用途 | 手键练习；Digimode 内部也走 CW 发射通路（由电脑精确控频，侧音会被接管） |

### 6.3 AM / USB 话音发射

相对原版「非 FM 常禁发」，本固件允许在 **AM、USB** 下按 PTT 发话。  
音质与占空取决于硬件与场景；日常对讲仍建议 **FM**。实验性 DSB-SC 发射**未包含**在当前固件中。

### 6.4 AM Fix

菜单 **AM Fix**（若存在）：改善 AM 接收的已知增强，与原 egzumer 行为一致，建议 AM 收听时保持开启（视你菜单中的 ON/OFF）。

---

## 7. APRS 与 Digipeater（DgPeat）

### 7.1 能做什么

- 侧键进入专用 APRS 屏后，RF 使用**当前选中 VFO**（`TX_VFO`）的收发频率与功率档；并**临时**切到 **FM + 宽带 + 关亚音/DCS + 关压扩**（不沿用原调制/带宽/亚音），武装机内 Bell202/FSK Modem（实现源自 **ta1js**）收 APRS。  
- Digipeat / echo 转发也在该当前 VFO 频率上、按当前功率档发射。  
- 使用前请先把**要守听的那路 VFO**调到当地 APRS 信道并选中（常见建议如 **144.640 MHz**，以当地规定为准）。  
- 解码成功后屏幕显示：  
  - 第 1 行：源呼号（含 SSID）  
  - 第 2 行：Maidenhead 网格（由位置包推算；无则 `--------`）  
  - 第 3–6 行：注释 / 消息文本  
- 可按菜单配置是否、如何 **Digipeat（数传中继）**。

### 7.2 如何进入 / 退出

**进入：**

- 侧键动作选 **APRS**，或使用默认的 **下侧键长按（F2Long）**。  
- 再次按同一侧键可退出。  
- 若当时在 Digimode，进 APRS 会先退出 Digimode。  
- 进入时会快照当前 VFO 的频率/调制/带宽/亚音/压扩等，再应用临时 modem 射频（频率与功率保持当前选中 VFO）。  
- **首次进 APRS**：会锁定到选中的 `TX_VFO`，并强制 FM（以及宽带/关亚音等 modem 射频）。用于避免 Dual Watch / 跨段时 RX 仍停在另一路 VFO、导致首次无 RX 的情况。

**退出：**

- `EXIT`，或再次触发 APRS 侧键。  
- 退出后停听 Modem、**恢复进 APRS 前的 VFO 快照**，并回主界面。

### 7.3 界面与按键

| 按键 | 作用 |
|------|------|
| `EXIT` | 退出 APRS |
| `PTT` | **无效**（禁语音发射；转发由固件自动进行） |
| 其它键 | 一般无效（双音提示） |

界面无手动发射按钮：本模式侧重 **监听 + 可选自动转发**，不是完整 APRS 手持终端（无菜单里设坐标发信标等）。

**屏内行为（相对主界面）：**

| 项 | 行为 |
|----|------|
| Dual Watch | **禁用**（不在 A/B 间切换；始终守当前选中 VFO） |
| PTT / VOX | **禁止**语音发射（VOX 路径在 APRS 屏直接返回） |
| 静噪结束 | **不拆** FSK modem（不会因 squelch lost 走完整 `RADIO_SetupRegisters` 清掉 REG_3F / Modem） |
| 静噪 Sql | **沿用全局菜单 Sql**，进 APRS **不强制改成 1**；听不清包时可自行在主菜单调 Sql |

### 7.4 Digipeater 菜单

| 菜单名 | 作用 | 可选值 / 范围 | 默认（全新） |
|--------|------|----------------|--------------|
| **DgPeat** | 转发模式 | OFF / **n-N** / **echo** | **OFF** |
| **DgCall** | 本机 Digi 呼号（DigiCall） | 最多 6 字符 A–Z/0–9 | 空 |
| **DgSSID** | Digi SSID | 0–15 | 0 |
| **WIDE** | 响应哪些 WIDE | **1** / **2** / **1+2** | **1**（仅 WIDE1-1） |

DigiCall / DgSSID / DgPeat / WIDE 等存在 EEPROM **`0x0F20`** 段（与 MyCall `0x0E30` 独立）。

#### DgPeat 模式含义

| 模式 | 用户理解 |
|------|----------|
| **OFF** | 只收显示，**不转发** |
| **n-N** | 规范 Digipeater：按路径改写（插入/标记你的 DgCall，处理 WIDE1/WIDE2 等），有约 **30 秒** 重复包抑制 |
| **echo** | 收到合法帧后 **原样重发**（调试/实验用；易造成环路，慎用） |

#### n-N 使用注意

1. **必须先设 DgCall**；呼号为空时无法进入有效 n-N（保存时空呼号会强制回到 OFF）。  
2. **WIDE** 决定响应 WIDE1、WIDE2 或两者。  
3. 不转发「已经是你自己发出」的包；路径已处理过你的呼号时也不重复插。  
4. 信道忙时会延后重试；转发前后有短冷却，降低自激。

### 7.5 推荐使用步骤（监听）

1. 菜单确认 **DgPeat = OFF**（只听）；按需调好全局 **Sql**（不必特意设为 1）。  
2. 选中要守听的 VFO，调到当地 APRS 频率与所需功率。  
3. 长按下侧键（或你绑的 APRS 键）进入。  
4. 等到附近台发 APRS，看呼号 / 网格 / 注释。  
5. `EXIT` 退出（调制/带宽/亚音等会恢复进屏前快照）。

### 7.6 推荐使用步骤（正规 Digipeater）

1. **DgCall** 填入你的呼号（须已获许可并符合当地标识要求）。  
2. **DgSSID** 按需（常用如 7、10 等，按你习惯）。  
3. **WIDE** 选 `1`、`2` 或 `1+2`。  
4. **DgPeat** 选 **n-N**。  
5. 进入 APRS 界面保持监听；听到包且路径匹配时会自动转发。  
6. 不用时务必将 **DgPeat 改回 OFF**，避免无人值守误转发。

### 7.7 限制与注意

- 进 APRS 期间使用**当前选中 VFO 的频率与功率**，并临时强制 FM/宽带/关亚音/关压扩（退出后恢复快照）；本版不在 APRS 界面内改频。  
- Dual Watch 开着时也能进 APRS：进屏后会锁到选中 VFO；若首次仍无 RX，确认选中的是 APRS 那路 VFO 后重新进一次。  
- 依赖机内 FSK Modem，弱信号、干扰下可能解不出；可调全局 Sql，但开太松会更易被噪声打断观感。  
- 支持常见位置包、Mic-E、部分消息文本显示；不能当作完整 APRS 客户端。  
- **echo** 仅建议短时测试。  
- 测时序 / 听转发时建议拔掉编程串口（见 FAQ）。

---

## 8. 精简广播 FM

### 8.1 进入 / 退出

- **进入 / 退出：** `F` + `0`  
- **退出也可：** FM 界面按 `EXIT`

### 8.2 操作

| 按键 | 作用 |
|------|------|
| `UP` / `DOWN` 短按或长按 | 频率 ±0.1 MHz，到端绕回 |
| `EXIT` | 退出 FM |
| 数字键 / `PTT` / 侧键 / `MENU` 等 | **无效**（精简策略） |

### 8.3 频率范围与默认行为

| 项 | 值 |
|----|-----|
| 范围 | **64.0 – 108.0 MHz** |
| 步进 | 0.1 MHz |
| 信道记忆 / 自动扫描 / 存台 | **已移除** |
| 侧键进 FM | **不可用** |

频率会写入 FM 相关 EEPROM，下次 `F+0` 从上次频率附近继续（仍被限制在 64–108）。

---

## 9. Digimode（电脑控频数字发射）

### 9.1 是什么

电台通过编程 UART 接收电脑指令，做**亚赫兹级**频率步进的恒包络 FSK 发射（典型用途：VHF/UHF 上的 **FT8** 等）。符号由电脑生成，电台负责定时与射频。

### 9.2 如何进入

- **没有**单独的菜单项「进 Digimode」。  
- 当电脑工具发出 Digimode 协议帧（同步字节 `0xAB`，命令 `0x01`–`0x0A`）时，电台自动进入，屏幕切换为 DIG 界面。  
- 与原厂刷写协议（`0xAB 0xCD`）可共用串口、靠第二字节区分。

### 9.3 屏幕含义

| 显示 | 含义 |
|------|------|
| **DIG RX** | 已进模式，未在发射 |
| **DIG TX** | 正在发射 |
| **DIG WAIT** | 已排程，等待开始时刻（显示倒计时） |
| 大号频率 | 基频 |
| AF: … Hz | 当前音频偏移 |
| RF: … | 实际射频 |
| FIFO / CRC | 队列深度与近期校验失败计数（排查链路用） |

### 9.4 退出

- 按 **`EXIT`** 干净退出 Digimode，恢复正常收音状态。  
- 链路心跳约 **1 s** 无有效通信会停发射（防电脑掉线一直占频）。  
- 进 APRS 时也会先退出 Digimode。

### 9.5 电脑工具位置（仓库内）

| 路径 | 用途 |
|------|------|
| `tools/digimode/` 或 `tools/gui/` | Windows GUI（v1 音频桥、v2 手动 FT8、v3 WSJT-X UDP） |
| `tools/digimode/ft8_send_*.py` 等 | 命令行 / 批处理发送 |
| `tools/DIGITAL_MODE.md` | 协议与进阶说明（偏技术） |

典型流程（概念）：

1. 电台开机，接好编程线或 USB 串口适配器。  
2. 电脑打开对应 GUI / 脚本，选对端口、**38400**。  
3. 用工具对时 → 排程或实时发符号。  
4. 电台显示 DIG；结束后按 `EXIT` 或让工具停 TX。

### 9.6 注意

- Digimode 与 **CAT 同时进入**会冲突：CAT 进入时若已在 Digimode 会失败。  
- 发射前确认频率、功率合法。  
- 占线时请勿用 CPS 同时读写。

---

## 10. CAT 遥控

### 10.1 是什么

电脑通过同一编程 UART 读写**当前 VFO**（频率、音调、功率、静噪、VOX 等），便于遥控与简易网控。  
**无独立 CAT 全屏界面**；进入后仍在主界面，状态栏出现 **`CAT`** 字样。

### 10.2 如何进入 / 退出

- 由电脑工具发送 CAT 命令（同步 `0xAB`，命令约 `0x10`–`0x1B`）。  
- 进入时：若当前是信道模式，会**强制切到 VFO（频率）模式**。  
- 退出时：VFO 上的改动**保留**（不做整机备份还原）。  
- 心跳超时约 **5 s** 无通信时，固件侧会按协议逻辑处理链路（请用官方工具保持心跳）。

### 10.3 电脑工具

| 路径 | 用途 |
|------|------|
| `tools/cat_control/cat_cli.py` | 命令行：`python cat_cli.py -p /dev/ttyUSB0`（默认 38400） |
| `tools/cat_control/webui/` | 浏览器界面（信道预设、状态、RSSI 等） |
| `tools/cat_control/run_webui.sh` | 辅助启动 Web UI |

CLI 常用概念命令：`freq`、`txfreq`、`offset`、`power`、`squelch`、`vox`、`status`、`quit` 等（详见脚本内 `help`）。

### 10.4 可遥控参数（摘要）

收/发频率、频差方向与大小、CTCSS/DCS、调制（FM/AM/USB）、功率、带宽、静噪、VOX、麦克风增益、扬声器相关增益、压扩、扰频、忙锁、步进；只读含 RSSI、mic 电平、状态（收发、电池 mV 等）。

功率在协议里可按多档编号传递，固件映射到本机 LOW / MID / HIGH。

### 10.5 注意

- 已在 Digimode 时无法进入 CAT。  
- CAT 改的是当前 VFO，退出后设置仍在——遥控前看清频率再按 PTT。

---

## 11. DTMF 与 Spectrum（默认关闭）

### 11.1 DTMF

默认固件**不含 DTMF 功能**（菜单无 UPCode / PTT ID / D Live 等，主界面无 DTMF 拨号态）。  
若你需要 DTMF，须自行用 `ENABLE_DTMF=1`（及可选 `ENABLE_DTMF_CALLING=1`）重新编译；本手册描述默认发布行为。

### 11.2 Spectrum

默认**无频谱仪**：

- `F+5` 不会进入频谱（在无 NOAA 的配置下该键行为也不同于原版频谱快捷键）。  
- 侧键列表无 SPECTRUM。  

需要频谱须以 `ENABLE_SPECTRUM=1` 重新编译。

---

## 12. 其它相对原版的实用差异

| 项目 | 说明 |
|------|------|
| 侧键列表无 FM | 见第 8 章 |
| AM/USB/CW 可发 | 见第 6 章 |
| 打包脚本 | 仓库提供 `build-packed.sh` 便于生成可刷 packed 固件（给编译者，不是机内菜单） |
| 状态栏 CAT | 见第 10 章 |

日常对讲、信道、扫描、双守候、CTCSS 等未点名的行为，大体仍与 egzumer 手册一致，请参考上游 Wiki；本手册只覆盖相对基线的增量与默认差异。

---

## 附录 A. 本固件相关菜单项一览

（默认编译：DTMF 关、Spectrum 关、APRS / Flashlight 开）

| 菜单显示 | 说明 |
|----------|------|
| Beacon | 障碍灯慢闪 OFF/ON |
| DgPeat | APRS 转发 OFF / n-N / echo |
| DgCall | Digipeater 呼号 |
| DgSSID | Digipeater SSID 0–15 |
| WIDE | 1 / 2 / 1+2 |
| Roger | OFF / ROGER / MDC / **MORSE**（MORSE 用 MyCall；750 Hz Tone1+TxMute） |
| MyCall | 莫尔斯 Roger 呼号（≤6；与 DgCall 独立；EEPROM `0x0E30`） |
| Sql | 全局静噪；APRS 屏沿用，不强制改 |
| Demodu | 含 **CW** |
| F1Shrt / F1Long / F2Shrt / F2Long / M Long | 侧键与 MENU 长按；可选 **APRS** |
| AM Fix | AM 接收增强 |

隐藏菜单（开机按住 PTT + 上侧键等传统方式）仍含 F Lock、扩展发射段、Reset 等，与原版类似，请谨慎操作。

---

## 附录 B. 用户可配 EEPROM 项（呼号等）

下列地址供备份/对照；日常请用菜单修改，无需手改 EEPROM。

| 内容 | EEPROM 区域 | 说明 |
|------|-------------|------|
| **MyCall** | `0x0E30`–`0x0E37` | 最多 6 字符 A–Z/0–9；仅 Morse Roger；与 DgCall 无关 |
| **APRS Digi** | `0x0F20`–`0x0F2F` | flags、SSID、DgCall；**勿误改 `0x0F40` 段其它位含义** |
| **Beacon** | `0x0F40` 字节 bit0（反相存储） | 全新 `0xFF` → Beacon OFF |
| 侧键动作 | `0x0E90` 段 | 含 F2 长按；APRS 构建下无效值回落为 APRS |
| Roger 模式 | 原 Roger 字节（与其它杂项同区） | 取值 0–3：OFF / ROGER / MDC / **MORSE** |

**Reset：**

- **VFO**：保留 MyCall（`0x0E30`）与 Digi（`0x0F20`）等「空洞」数据。  
- **ALL**：大范围擦除，MyCall / Digi 等回到空 / OFF。

---

## 附录 C. 常见问题

**Q1：长按下侧键没进 APRS？**  
检查菜单 **F2Long** 是否为 **APRS**。若被改成其它功能，改回即可。

**Q2：进了 APRS 没声音？**  
正常。APRS 监听时关闭扬声器音频通路，靠 Modem 收包；界面有呼号/网格即表示在工作。PTT / VOX 在 APRS 屏无效，不能靠按 PTT「听开静噪」。

**Q3：n-N 不转发？**  
确认 DgPeat=n-N、DgCall 非空、WIDE 与路径匹配、不是重复包（30 s 内）、信道空闲。只听请用 OFF。

**Q4：Morse Roger 没声音？**  
确认 Roger=**MORSE**、**MyCall 非空**（空则故意静默）、当前为 **FM**（CW/AM/USB 不走 Roger）。莫尔斯很快（约 100 WPM、Tone1 **750 Hz**），注意听松 PTT 瞬间。MyCall 与 DgCall 无关，改 Digi 呼号不会影响 Morse。

**Q5：Beacon 不闪？**  
Beacon=ON，且手电处于关闭（不是常亮/SOS）。节奏约 120 ms 亮、约 5 s 周期。

**Q6：FM 侧键没反应 / 不能存台？**  
精简 FM：只能 `F+0`，无信道与扫描。

**Q7：找不到频谱 / DTMF？**  
默认编译已关闭。见第 11 章。

**Q8：Digimode / CAT 连不上？**  
检查端口、**38400**、是否被 CPS 占用；两者不要叠用；先 `EXIT` 退出 Digimode 再开 CAT。

**Q9：测 APRS 转发时序时数据怪、延迟不稳？**  
请**拔掉编程线 / AIOC 串口**，关掉电脑串口程序后再测。串口通信与 USB 声卡一体线可能干扰接收/占用，影响你对「听包→转发」时间的判断。

**Q10：刷机后菜单乱码或奇怪选项？**  
做一次菜单 **Reset**（先试 VFO，不行再 ALL），并重新设置 MyCall / DgCall / 侧键。

**Q11：能否当完整 APRS 手持发信标？**  
不能。当前是当前 VFO 频率监听 + 可选 Digipeater + 简易显示，无菜单编辑坐标/路径发自己的位置包。

**Q12：Dual Watch / 跨段时进 APRS 收不到？**  
进 APRS 会锁到**当前选中（TX）VFO**并强制 FM。请先把 APRS 那路 VFO 选中再进；屏内 Dual Watch 不会切换。若仍异常，`EXIT` 后重新进一次。

**Q13：APRS 的 Sql 要设成 1 吗？**  
不必。沿用菜单全局 **Sql**，固件进 APRS **不会**强制改 Sql。

---

## 附录 D. 快速对照卡

```
F+0          广播 FM（64–108，上下步进，EXIT 退出）
F2 长按*     APRS（选中 VFO 频率/功率；临时 FM/宽/关亚音；禁 DW/PTT/VOX；*默认）
EXIT         退出 APRS / Digimode / FM（APRS 退出恢复快照）
菜单 Beacon  障碍灯慢闪（约 5 s 一闪）
菜单 DgPeat  OFF | n-N | echo；WIDE 1/2/1+2；DgCall@0x0F20
MyCall(≤6@0x0E30) + Roger=MORSE   FM松PTT：750Hz Tone1+TxMute（空静默）
Demodu=CW    真 CW 收发（侧音 650 Hz；与 Morse Roger 不同）
电脑串口     Digimode（FT8 等）/ CAT（状态栏 CAT）
```

---

*文档文件：`docs/USER_MANUAL_BG7NZL_ZH.md`（中文）；英文版：`docs/USER_MANUAL_BG7NZL.md`。功能以当前树源码与默认 `Makefile` 为准；若你使用自定义编译开关，菜单与侧键可能与本文不完全一致。*
