---
title: "I2C Protocol Presentation"
date: 2024-10-03T16:13:37+08:00
draft: true
---
# AGENDA

1. I2C 協定介紹
2. 基於2.1版本的 I2C 協定概述
    * Transmission Speeds
    * I2C 實體層
    * 主從架構 Master-Slave Architecture
    * 位元傳輸
    * 傳輸資料 TRANSFERRING DATA
        * START 與 STOP 條件 (START Condition & STOP Condition) 
            * Repeated START 條件
        * Byte format
        * Acknowledge (ACK) and Not Acknowledge (NACK)
    * 尋址 Addressing Scheme
        * 7-bit Addressing
        * First Byte 
            >　結構與功能，並搭配時序圖展示其在通信中的作用。
        * 10-bit Addressing
    * Acknowledgement Mechanism (ACK/NACK)
    * Arbitration and Synchronization
    * Multi-Master Configuration
    * Error Detection and Handling
3. I2C Protocol Challenges and Limitations
4. I2C vs Other Communication Protocols (SPI/UART)

# I2C 協定介紹

I²C（Inter-Integrated Circuit）是一種由飛利浦半導體（現為恩智浦半導體 NXP Semiconductors）於1982 年發布的串行通訊協議。I²C協議的主要制定者是飛利浦，目前則由恩智浦負責維護。該協議最初設計的目的是讓微控制器（MCU）可以簡單且低成本地與其他元件（如傳感器、顯示器和存儲設備等）進行通信。

I²C 使用兩條信號線——數據線（SDA）和時鐘線（SCL）來傳輸數據，具有同步且雙向的特性。I²C 可以支持多個主設備與從設備之間的通訊，因此適用於多數嵌入式系統和消費性電子產品中，特別是在需要多元件之間低速通訊的場景。I2C 用於各種控制架構，例如係統管理匯流排（SMBus）、電源管理匯流排 (PMBus)、智慧型平台管理介面 (IPMI)、顯示器資料通道 (DDC) 和高階電信運算架構 (ATCA) 

## I2C 版本演進

![I2C Version History](/images/figure_1.png)

<table>
    <tr>
        <td>Version</td>
        <td>主要重點</td>
    </tr>
    <tr>
        <td>First</td>
        <td>The 1st version of I2C protocol。</td>
    </tr>
    <tr>
        <td>1.0</td>
        <td>
        <li>100kHz 以下的模式, 統一稱為<b>標準模式</b> 。</li>
        <li>新增<b>快速模式</b> 400kHz, 並兼容標準模式。</li>
        </td>
    </tr>
        <tr>
        <td>2.0</td>
        <td>新增<b>高速模式</b> 3.4MHz. 兼容標準模式. <b>10位地址首次被提及</b>。</td>
    </tr>
    </tr>
        <tr>
        <td>2.1</td>
        <td>
        <li>繼續支援7位元和10位元位址，並詳細規定了位址分配和衝突解決的機制。</li>
        <li>增加了一些擴展特性，如主機和從機之間更靈活的握手和應答機制，以適應不同的應用需求。</li>
        <li>對不同功能的設備進行了更詳細的描述，幫助開發人員更好地理解和實現I²C協議。</li>
        </td>
    </tr>
    </tr>
        <tr>
        <td>3.0</td>
        <td>
        <li><b>快速模式+</b> 1MHz。</li>
        <li>此版開始由 NXP 發布。</li>
        </td>
    </tr>
    </tr>
        <tr>
        <td>4.0</td>
        <td>
            <li><b>Ultra Fast-mode</b> 5MHz, 以及相關的 USDA, USCL 介紹，不使用 I²C 標準的雙向線路，僅限於單向資料傳輸，適用於一些特別高需求的資料傳輸應用。</li>
            <li>Display Data Channel DDC 介紹。</li>
        </td>
    </tr>
    </tr>
        <tr>
        <td>5.0</td>
        <td>Version 4.0 修改</td>
    </tr>
    </tr>
        <tr>
        <td>6.0</td>
        <td>更新了規格和部分細節，改進了對高速訊號的支持，增強了訊號完整性，以適應日益複雜的嵌入式系統需求</td>
    </tr>
    </tr>
        <tr>
        <td>7.0</td>
        <td><b>MIPI I3C</b></td>
    </tr>
</table>

# 基於2.1版本的 I2C 協定概述

## I2C 傳送速率

| 速率模式        | 傳輸速率    | 應用範圍                     | 特點                     |
|-----------------|-------------|-----------------------------|--------------------------|
| **標準模式**     | 100 kbps    | 低速嵌入式系統，低功耗設備   | 穩定、功耗低             |
| **快速模式**     | 400 kbps    | 顯示器、傳感器等中等帶寬需求 | 較高的數據速率，穩定性良好|
| **超快速模式**   | 3.4 Mbps    | 音頻/視頻設備，高性能設備   | 高數據速率，距離受限     |
| **快速模式+** (Ver 3.0)   | 1 Mbps      | 工業控制、高效能傳感器      | 高速傳輸，穩定性優於超快速模式|
| **超高速模式** (Ver 4.0)  | 5 Mbps 或更高| 高解析度圖像傳輸，先進通信   | 極高速率，距離有限，僅限於單向資料傳輸    |

## I2C 實體層

![I2C Physical Layer](/images/figure_8.png)

### SDA 和 SCL 的功能
* SDA (Serial Data Line)
    1. 負責傳輸數據（包括位址、命令和數據內容）。
    2. **雙向**數據線：可以作為 主機到從機 或 從機到主機 的傳輸媒介。
* SCL (Serial Clock Line)
    1. 生成同步時鐘信號，用於協調數據的傳輸。
    2. 僅由**主機**生成時鐘信號，從機被動接收。

### 特性
* 雙線結構
    1. I²C 僅使用 SDA 和 SCL 兩條線實現所有通信，**簡化**了硬體設計。
* 開漏輸出（Open-Drain 或 Open-Collector）：
    1. SDA 和 SCL 均採用**開漏輸出**結構，線路上的電壓狀態由**外部拉高電阻（Pull-up Resistor）**維持。
    2. 每個設備僅能通過拉低電平（邏輯 0）來**主動**控制線路，而邏輯 1 則依賴拉高電阻自動回到高電位。
* 多設備共享總線
    1. 所有連接到 I²C 線路的設備共享相同的 SDA 和 SCL，實現多主多從通信架構。

### SDA 和 SCL 邏輯電平

<table>
    <tr>
        <td>    </td>
        <td>High Level</td>
        <td>Low Level</td>
        <td>低壓支持 </td>
    </tr>
    <tr>
        <td>Fixed Input Levels </br>
        (僅 Standard Mode 支援)</td>
        <td> > 3.0V </td>
        <td> < 1.5V </td>
        <td>否 </td>
    </tr>
    <tr>
        <td>VDD-related Input Levels </br>
        ( F/S Mode, HS Mode)</td>
        <td> > 0.7*VDD </td>
        <td> < 0.3*VDD </td>
        <td>是 </td>
    </tr>
</table>

## 主從架構 Master-Slave Architecture

![Master-Slave Architecture](/images/figure_9.png)

主從式架構：主機負責生成時鐘並初始化通訊，從機被動響應。

### 主從架構

* Master（主機）
    * 負責控制 I²C 線路上的通信流程。主機的職責包括：
        * 生成時鐘信號（SCL）。
        * 發送起始條件（Start Condition）和停止條件（Stop Condition）。
        * 發起數據傳輸，選擇目標從機，並決定傳輸方向（讀/寫）。
* Slave（從機）
    * 被動響應主機的指令。其角色包括：
        * 接收位址匹配後進行數據傳輸。
        * 返回應答信號（ACK/NACK）以確認是否成功接收數據。

### 多主多從支持

I²C 協議支持多主多從架構（Multi-Master, Multi-Slave），但多數實際應用中只有一個主機。

## 位元傳輸

位元傳輸 是 I²C 協議中最基本的數據交換單位，描述了如何在總線上以比特為單位進行數據的傳遞。

### 位元傳輸過程：
* 數據位（Data Bit）傳輸
    * 在 I²C 中，每個數據位的傳輸是在 SDA 線（數據線）上進行的，而傳輸的時序由 SCL 線（時鐘線）提供。
    * 每個數據位的傳輸過程如下
        1. 主機生成時鐘，通過 SCL 線產生時鐘信號，並以此為基準同步 SDA 線的數據傳輸。
        2. SDA 線上的數據變化：數據位是根據 SCL 時鐘信號的邊緣（上升或下降）來傳送的。每當 SCL 為高時，SDA 的狀態會被讀取。
        3. 數據位的順序：數據位按照 MSB (Most Significant Bit) 先傳送的規則進行傳輸。
### 位元傳輸的時序圖：
* 提供一個 時序圖，顯示如何在每一個 SCL 時鐘週期內傳輸一個數據位。
    * 上升沿：SDA 線的變化表示數據位的開始。
    * 下降沿：SCL 線的變化同步數據位的傳送。
### 位元的有效性與同步性
* 數據位的有效性：每個數據位只有在相應的 SCL 信號的邊緣（上升或下降）才被認為是有效的。這保證了數據位的同步性和正確性。
* SDA 與 SCL 的同步：SDA 線的狀態變化必須在 SCL 為低時進行，並在 SCL 為高時保持穩定。這樣的設計確保了數據的準確傳輸。

![Data validity](/images/figure_2.png)


## TRANSFERRING DATA

I²C 資料傳輸主要由 START 條件、ADDRESS BYTE、DATA BYTE、ACK/NACK 位元，以及 STOP 條件等型態組成。

| 信號類型         | 由誰發出               | 作用及說明                                               |
|------------------|------------------------|--------------------------------------------------------|
| **START**        | 主機 (Master)          | 主機開始傳輸，告訴從機通信即將開始。此信號不由 SDA 或 SCL 發送數據，而是通過 SDA 線由高到低的過渡來表示。 |
| **STOP**         | 主機 (Master)          | 主機結束傳輸，通知從機數據傳輸結束。此信號由 SDA 線由低到高的過渡來表示。        |
| **ADDRESS BYTE** | 主機 (Master)          | 主機發送地址字節給從機，這通常包括 7 位或 10 位地址，確定通信的目標從機。 |
| **DATA BYTE**    | 主機 (Master) 或 從機 (Slave) | 主機發送數據字節給從機，或者從機發送數據字節給主機。兩者都可以在通信過程中傳送數據字節。 |
| **ACK**          | 從機 (Slave)           | 當從機成功接收到字節並準備接收下一個字節時，從機拉低 SDA 線發送 ACK (0)。 |
| **NACK**         | 從機 (Slave)           | 當從機不再接收數據或在接收過程中發生錯誤時，從機保持 SDA 線為高電平發送 NACK (1)。 |

#### 備註：
1. **START** 和 **STOP** 信號由主機控制，主機決定何時開始或結束通信。
2. **ADDRESS BYTE** 和 **DATA BYTE** 由主機發送，但在讀取模式下，數據字節也可以由從機發送。
3. **ACK** 和 **NACK** 信號由從機發送，主機根據從機的反應來判斷是否繼續傳輸或結束。

### START 與 STOP 條件 (START Condition & STOP Condition) 

1. 由**主機**產生 START 和 STOP 條件。
2. 在 **START** 條件發生後，總線被視為 **busy**。在 **STOP** 條件發生後的某個時間，總線被視為再次 **空閒**。
3. 如果產生 **repeated START (Sr)** 而不是 STOP 條件，總線將保持忙碌狀態。 START (S) 和 repeated START (Sr) 條件在功能上是相同的。

![START and STOP conditions](/images/figure_3.png)

### Byte format

1. SDA 線上的每個位元組必須是 **8 位元**長。每次傳輸可以傳輸的位元組數不受限制。每個位元組後面都必須有一個確認位。資料首先傳輸最高有效位元 (MSB)。
2. 如果從設備在執行某些其他功能（例如服務內部中斷）之前無法接收或傳輸另一個完整的資料位元組，則它可以**將時脈線SCL 保持為低電平**，以強制主設備進入 **等待狀態**。

![Byte format](/images/figure_10.png)

### Acknowledge (ACK) and Not Acknowledge (NACK)

1. 已定址的接收器必須在接收到每個位元組後產生**確認(ACK)**。
2. **資料傳輸**必須經過確認。與確認相關的時脈由主機產生。發送器釋放SDA線（HIGH）在**確認時脈期間**。
3. 接收器必須在應答時脈時拉低 SDA 線，以便在該時脈的高電位期間保持穩定的低電位。當然，也必須考慮設定和**保持時間**。
4. 當從機不確認從機位址時（例如，因正在執行某些即時功能而無法接收或傳送），從機必須將資料線保持為高電位。然後，主機可以產生一個 STOP 條件來中止傳輸，或產生一個重複的 START 條件來開始新的傳輸。
5. 如果從機接收器確實確認了從機位址，但在傳輸一段時間後無法接收更多資料位元組，則主機必須再次中止傳輸。
6. 從傳送器必須**釋放資料線**以允許主機產生 STOP 或 repeated START 條件。

![Acknowledge](/images/figure_4.png)


## 尋址 Addressing Scheme

Addressing Scheme 指的是 I²C 協議中如何為每個設備分配並識別唯一位址的規則和結構。由於 I²C 是一個多設備總線協議，主機需要通過設備的位址來選擇目標設備進行通信。因此，Addressing Scheme 描述了如何在總線上對每個從機設備進行標識，並確保每個設備在同一總線上的唯一性。

在 I²C 中，主要有兩種尋址方案：7-bit 尋址 和 10-bit 尋址。


### 7-bit 尋址
### FIRST Byte
### 10-bit 尋址


### I2C 總線協定特性的適用性 (Ver7, 可移到最後面)
<table>
    <tr>
        <td>特徵 Feature</td>
        <td>單主控 Single controller</td>
        <td>多主控 Multi-controller</td>
        <td>目標 Target (Controller or Slave Deice)</td>
    </tr>
    <tr>
        <td>START 條件</td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
    </tr>
    <tr>
        <td>STOP 條件</td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
    </tr>
    <tr>
        <td>認可 Acknowledge</td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
    </tr>
    <tr>
        <td>同步 Synchronization</td>
        <td>n/a</td>
        <td><font color=#0050A0>必要項</font></td>
        <td>n/a</td>
    </tr>
    <tr>
        <td>仲裁協定 Arbitration</td>
        <td>n/a</td>
        <td><font color=#0050A0>必要項</font></td>
        <td>n/a</td>
    </tr>
    <tr>
        <td>時脈擴展 Clock stretching (1)</td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
    </tr>
    <tr>
        <td>7-bit 尋址</td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
        <td><font color=#0050A0>必要項</font></td>
    </tr>
    <tr>
        <td>10-bit 尋址</td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
    </tr>
    <tr>
        <td>General Call address*</td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
    </tr>
    <tr>
        <td>Software Reset</td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
        <td><font color=#00A050>可選項</font></td>
    </tr>
    <tr>
        <td>START byte (2)</td>
        <td>n/a</td>
        <td><font color=#00A050>可選項</font></td>
        <td>n/a</td>
    </tr>
    <tr>
        <td>Device ID</td>
        <td>n/a</td>
        <td>n/a</td>
        <td><font color=#00A050>可選項</font></td>
    </tr>
</table>

1. Clock stretching is a feature of some targets. If no targets in a system can stretch the clock (hold SCL LOW), the
controller need not be designed to handle this procedure.
2. Bit banging’ (software emulation) **multi-controller** systems should consider a START byte.


















## ARBITRATION AND CLOCK GENERATION

### **Synchronization (同步)** - SCL

A procedure to synchronize the clock signals of two or more devices. The clock signals during arbitration are a synchronized combination of the clocks generated by the masters using the wired-AND connection to the SCL line.

1. All masters generate their own clock on the SCL line to transfer messages on the I2C-bus. Data is only valid during the HIGH period of the clock. A defined clock is therefore needed for the **bit-by-bit** **arbitration procedure** to take place.
2. Clock synchronization is performed using the **wired-AND** connection of I2C interfaces to the SCL line. This means that a HIGH to LOW transition on the SCL line will cause the devices concerned to start counting off their LOW period and, once a device clock has gone LOW, it will hold the SCL line in that state until the clock HIGH state is reached.
3. However, the LOW to HIGH transition if this clock may not change the state of the SCL line if nother clock is still within its LOW period. The SCL line will therefore be held LOW by the device with the **longest LOW** period.
4. Devices with shorter LOW periods enter a **HIGH wait-state** during this time.
5. The **first** device to complete its HIGH period will again pull the SCL line LOW.
6. In this way, a **synchronized SCL clock** is generated with its **LOW period** determined by the device with the **longest clock LOW period**, and its **HIGH period** determined by the one with the **shortest clock HIGH period**.

* 目標：協調多個主機的 SCL（時鐘線）訊號，確保各主機的時鐘訊號能正確配合。
* 原理：
    * I²C 協議要求 SCL 是 線與（wired-AND） 訊號，任何主機都可以將 SCL 拉低。
    * 當多個主機同時操作 SCL 時，實際上的 SCL 訊號是由所有主機的時鐘信號疊加的結果。
    * 主機需要等待 SCL 釋放為高電平才能繼續下一個時鐘週期，這種機制自然地實現了時鐘同步。

![Synchronization](/images/figure_5.png)

#### **Wired-AND connection** - ??

### **Arbitration (仲裁)** - SDA

A procedure to ensure that, if more than one master simultaneously tries to control the bus, only one is allowed to do so and the winning message is not corrupted. If two or more masters try to put information onto the bus, the first to produce a ‘one’ when the other produces a ‘zero’ will lose the arbitration.

1. A master may start a transfer only if the bus is free.
2. Two or more masters may generate a START condition within the minimum hold time (tHD;STA, Hold time after START condition) of the START condition which results in a defined START condition to the bus.
3. Arbitration takes place on the SDA line, while the SCL line is at the HIGH level, in such a way that the master which transmits a **HIGH level**, while another master is transmitting a **LOW level** will switch off its DATA output stage because the level on the bus doesn’t correspond to its own level.

* 目標：確保在多個主機（Master）同時試圖控制匯流排時，只有一個主機能夠繼續進行傳輸，而其他主機會停止操作。
* 原理：
    * 仲裁發生在 SDA（數據線）上。
    * 主機透過發送數據並同時監聽 SDA 線的電平來進行仲裁。
    * 如果某個主機發送的是 1（釋放 SDA），但讀到的是 0（另一主機拉低 SDA），那麼它會退出仲裁，讓位於持續主導 SDA 的主機。
* 結果：勝出的主機繼續完成它的資料傳輸，其他主機將進入等待。


![Arbitration](/images/figure_6.png)

### The difference between Arbitration and Synchronization

<table>
    <tr>
        <td>功能</td>
        <td>Arbitration （仲裁）</td>
        <td>Synchronization （同步）</td>
    </tr>
    <tr>
        <td>發生對象</td>
        <td>SDA（數據線）</td>
        <td>SCL（時鐘線）</td>
    </tr>
    <tr>
        <td>解決問題</td>
        <td>確定哪個主機擁有總線控制權</td>
        <td>協調多主機時鐘信號</td>
    </tr>
    <tr>
        <td>發生條件</td>
        <td>多主機同時發送數據</td>
        <td>多主機時鐘信號存在差異</td>
    </tr>
    <tr>
        <td>結果</td>
        <td>一個主機獲勝，其他主機退出</td>
        <td>所有主機的時鐘自動同步</td>
    </tr>
</table>

#### 是否可能 SDA 和 SCL 由不同的主機產生？

**不可能**

根據 I²C 的設計規範，SDA 和 SCL 不能分別由不同的主機產生。這是因為：
1. 仲裁機制確保只有一個主控設備可以操作匯流排（SDA 和 SCL），其他主控設備會停止操作。
2. 如果 SDA 和 SCL 分別由不同主機控制，會破壞仲裁和同步機制，導致數據傳輸失敗或錯誤。
3. 若 SDA 和 SCL 同時由不同的主控設備驅動，I²C 匯流排上的邏輯會失效，導致資料錯誤或匯流排鎖死。
4. 同步機制確保時鐘（SCL）的穩定性，但這也間接要求同一個主控設備需要負責時鐘信號的驅動，否則可能導致資料傳輸中的一致性問題。

當多個主機試圖同時控制總線時，最終只有一個主機能通過仲裁獲得控制權。此時，該主機負責同時驅動 SDA 和 SCL，其餘主機停止傳輸並釋放總線。

### 總結

1. 仲裁 是解決多主控競爭的問題，確保匯流排由單一主控設備控制。
2. 同步 是確保時鐘線（SCL）的一致性，即使多個主控設備驅動也能協調運作。
3. 在 I²C 的設計下，SDA 和 SCL 不可能由不同的主控設備同時驅動。

#### 為什麼最後會由單一主控控制？
這是因為仲裁和同步兩者互相協作的結果：

1. 仲裁機制排除其他主控：
    * 仲裁確保匯流排上的 SDA 和 SCL 最終只受單一主控設備驅動。
    * 仲裁失敗的主控設備在仲裁結束後會釋放對匯流排的控制，不再驅動 SDA 和 SCL。
2. 同步機制不影響仲裁結果：
    * 同步只確保多主控設備在仲裁期間的 SCL 時序一致，並不改變 SDA 仲裁的結果。
    * 一旦仲裁結束，SCL 也會完全由仲裁勝出的主控設備驅動。
3. 多主控的設計避免衝突：
    * 仲裁和同步的結合避免了不同主控設備同時驅動 SDA 和 SCL 的情況。
    * I²C 的協定規範，仲裁失敗的主控設備必須立即停止驅動匯流排，確保只有仲裁勝出的主控設備保留控制權。

#### I²C 協議中仲裁、總線控制與特殊情況處理

I²C（Inter-Integrated Circuit）是一種多主多從（Multi-Master, Multi-Slave）的串列匯流排協議，支援多個主控設備（Masters）在單一總線上進行資料通訊。這段敘述主要涉及以下幾個重點內容：仲裁、無中央控制邏輯、以及特殊情況下的處理方式。

> Since control of the I2C-bus is decided solely on the address or master code and data sent by competing masters, there is no central master, nor any order of priority on the bus.
> Special attention must be paid if, during a serial transfer, the arbitration procedure is still in progress at the moment when a repeated START condition or a STOP condition is transmitted to the I2C-bus. If it’s possible for such a situation to occur, the masters involved must send this repeated START condition or STOP condition at the same position in the format frame. In other words, arbitration isn’t allowed between:

* A repeated START condition and a data bit.
* A STOP condition and a data bit.
* A repeated START condition and a STOP condition.

1. 仲裁的核心概念

    I²C 匯流排上，沒有中央控制器來決定哪個主控設備擁有優先權。控制匯流排的主控設備是根據仲裁機制決定的：
    * 仲裁條件：
        * 每個主控設備通過傳送 位址（Address） 和 資料（Data） 決定是否能持續掌控匯流排。
        * 仲裁過程只基於線上的 SDA 狀態：
            * 如果一個主控設備在輸出高電平（1）時，檢測到 SDA 線為低電平（0），它即退出仲裁。
            * 退出仲裁的設備會釋放 SDA 與 SCL，不再控制匯流排。
    * 結果： 最終只有一個主控設備贏得仲裁並繼續操作，而其他主控設備需等待匯流排釋放後再嘗試。

2. 無中央控制與無優先順序
    * 無中央控制器：
        * I²C 的設計沒有指定固定的優先級，任何設備都可以成為主控設備。
        * 主控設備是否能繼續操作，完全取決於它是否在仲裁過程中勝出。
    * 無優先順序：
        * 匯流排上的競爭是基於主控設備發出的資料內容（如地址或資料位），並無固定優先順序。
        * 這使得 I²C 匯流排更具靈活性，但也要求所有主控設備需依規範執行仲裁。

3. 特殊情況處理：重複 START 或 STOP 條件期間仲裁進行中

    問題描述： 
    當兩個或多個主控設備在傳輸過程中，仲裁尚未完成，而其中一個或多個主控設備試圖：
    1. 發送重複 START 條件（Repeated START）。
    2. 發送 STOP 條件。

    此時，可能會出現競爭情況，若處理不當，匯流排將無法正確同步。

    解決方法：

    1. 主控設備需同步重複 START 或 STOP 的位置：
        * 仲裁中，所有主控設備必須在相同的時序位置發送 START 或 STOP。
        * 這意味著當主控設備試圖發送重複 START 或 STOP 時，它們需確認自身仲裁的進程是否允許在正確的位元框架內操作。
    2. 若仲裁失敗的主控設備遇到 START/STOP：
        * 當主控設備偵測到自己失去仲裁控制，必須立即停止產生 START 或 STOP 信號，並釋放總線。
    3. 硬體與韌體協同設計：
        * 設備設計應確保硬體能根據 SDA 和 SCL 狀態快速決定是否停止操作。
        * 韌體邏輯需在傳送 START/STOP 信號前檢查匯流排狀態，避免不一致的條件發生。

#### 範例：重複 START 條件期間仲裁

假設有兩個主控設備（Master A 和 Master B）在同一匯流排上，且都試圖發送重複 START 條件：

1. Master A 發送位址「1010 0000」。
2. Master B 發送位址「1010 0011」。

##### 仲裁過程：

* SDA 線上按位比較發送內容：
    * 前七位地址（1010 000）完全相同，仲裁仍在進行中。
    * 第八位（讀/寫位）：Master A 發送 0，Master B 發送 1。
    * SDA 被 Master A 拉低（0），Master B 偵測到差異後退出仲裁。

##### 結果：

* Master A 繼續掌控匯流排，並發送重複 START。
* Master B 等待匯流排釋放後重試。

#### 總結

* 仲裁與同步機制確保 I²C 匯流排能在無中央控制的條件下運行。
* 主控設備需正確處理特殊情況（如仲裁期間的重複 START/STOP），避免操作不一致。
* 設備設計時，必須結合硬體快速響應能力與韌體邏輯同步，來實現協議的穩定運作。

### Use of the clock synchronizing mechanism as a handshake (內部使用)

保持 SCL LOW level 有兩種層面, 一個是 Byte 層面, 另一個是 bit 層面.
Byte 層面是發生在 Date 傳輸後, 接收端需要儲存或是反應結果, 需要相應的時間.
Bit 層面則是透過 Low level 來降低 SCL 的速度.



## FORMATS WITH 7-BIT ADDRESSES

## 7-BIT ADDRESSING

![First Byte](/images/figure_7.png)

### GENERAL CALL ADDRESS

### START Byte

#### START Byte 的作用
1. 加速多從設備的同步
* 在某些場景下，匯流排上有多個從設備（Slaves），它們需要快速捕捉主控設備的訊號。
* 通常，這些從設備可能需要以不同的速度同步到匯流排的操作。
* 使用 START byte，可以提醒所有從設備進入同步狀態，而不需要立即發送特定的目標位址。
2. 簡化從設備的設計
* 從設備可以設計為監聽 START byte，而不是立即處理後續的資料，從而降低對準確位址比對的即時需求。
* 特別是在多主控情境下，START byte 為從設備提供額外的緩衝時間來準備。
3. 支援高速通訊模式
* 在某些高效能的 I²C 系統中，START byte 可以用於啟動特定的快速操作模式（如快速初始化，或 Ultra Fast 模式）。

#### START Byte 的使用場景
* 多從架構：多個從設備需要監聽並同步到匯流排。
* 時鐘恢復：在匯流排上有可能的時鐘偏差情況下，START byte 幫助從設備重新同步。
* 設備啟動與初始化：作為初始化訊號，提醒從設備準備接受位址訊息或數據。

## I2C vs UART vs SPI




# Note
## Fault diagnosis and debugging are simple

## Malfunctions can be immediately traced






這是一個清晰的 I²C 協議報告大綱，涵蓋了技術層面及比較分析。以下是每個部分的重點及建議內容，幫助強化報告結構與內容深度：

1. Introduction to I2C Protocol
重點內容：

簡述 I²C 協議的由來，由 Philips 開發，主要用於低速通訊的嵌入式系統。
強調其特點：簡單的雙線協議 (SDA 和 SCL)、支持多主多從架構。
使用情境範例，例如感測器、EEPROM、顯示器等設備。
建議：

提供歷史背景及用途的時間軸圖表，幫助聽眾快速理解其演進與應用場景。
2. I2C Protocol Overview Based on Version 2.1
I2C Physical Layer
重點內容：

兩條線 (SDA 和 SCL) 具備雙向數據傳輸的能力，依賴於上拉電阻維持信號穩定性。
建議：

補充實際電路圖，標註 SDA、SCL 線路及其作用。
Transmission Speeds
重點內容：

不同速度模式：
標準模式 (100 kbps)
快速模式 (400 kbps)
快速模式+ (1 Mbps)
高速模式 (3.4 Mbps)
超快速模式 (5 Mbps，僅用於單向通訊)
建議：

使用表格清楚列出各模式的速度限制與應用範例，並強調 2012 年新增的超快速模式特性。
Master-Slave Architecture
重點內容：

單主與多主的架構，主設備負責生成時鐘信號和起始信號。
從設備只能回應，不會主動通訊。
建議：

使用示意圖描述多主多從情境，說明如何避免總線衝突。
Addressing Scheme
重點內容：

7-bit 與 10-bit 地址格式的差異及應用場景。
討論廣播地址 (0x00) 的特殊用法。
建議：

提供 I²C 地址分配的範例表（如 EEPROM、感測器常用地址）。
Data Transfer Format
重點內容：

起始條件、數據位元 (8-bit)、應答




---


以下是針對大綱中每個部分的重點內容與建議：

---

### **1. Introduction to I2C Protocol**
**重點：**
- 簡介 I²C (Inter-Integrated Circuit) 是一種串列通訊協議，由 Philips 在 1982 年設計。
- 應用範圍：嵌入式系統、感測器模組、顯示器等。
- 關鍵特性：雙線式通訊（SDA、SCL），支援多主機、多從機架構。

**建議內容：**
- 提供一個簡單的實際應用案例，例如用 I²C 接口連接溫度感測器。
- 使用示意圖說明 SDA 和 SCL 線如何實現通信。

---

### **2. I2C Protocol Overview Based on Version 2.1**
**重點：**
#### **I2C Physical Layer**
- 描述 SDA（數據線）和 SCL（時鐘線）的功能與特性。
- 解釋需要外部拉高電阻的原因。

#### **Transmission Speeds**
- 列出 I²C 支援的速度模式：
  - 標準模式（100 kbps）
  - 快速模式（400 kbps）
  - 高速模式（3.4 Mbps）
  - Ultra Fast Mode（5 Mbps，僅支援單向傳輸）。

#### **Master-Slave Architecture**
- 主從式架構：主機負責生成時鐘並初始化通訊，從機被動響應。

#### **Addressing Scheme**
- 討論 7 位元和 10 位元位址的差異。
- 說明如何避免位址衝突。

#### **Data Transfer Format**
- 包括開始位元、位址位元、數據位元、確認位元、結束位元。
- 圖解 I²C 數據帧結構。

#### **Start and Stop Condition**
- 說明如何產生 Start/Stop 條件（SDA 與 SCL 的電位變化）。

#### **Repeated Start Condition**
- 解釋多筆數據傳輸間的「重啟條件」用途。

#### **Acknowledgement Mechanism (ACK/NACK)**
- 描述從機如何回應主機的數據傳輸。

#### **Voltage Levels and Pull-up Resistors**
- 討論信號的邏輯電平與拉高電阻的重要性。

#### **Arbitration And Synchronization**
- 多主機情境下如何仲裁與同步。
- 示意圖展示仲裁失敗時主機的行為。

#### **Multi-Master Configuration**
- 支援多主機架構的設計與限制。

#### **Error Detection and Handling**
- 討論通訊錯誤的檢測方法，例如丟失 ACK 或時鐘同步問題。

**建議內容：**
- 在各小節提供簡潔的表格或圖表輔助說明。
- 加入與 2012 年 Ultra Fast 模式的關聯描述，強調其單向通訊應用。

---

### **3. I2C Protocol Challenges and Limitations**
**重點：**
- 討論 I²C 的限制，如速度相對較慢、電磁干擾敏感性。
- 分析多主機架構的實現困難（如仲裁問題）。
- 信號完整性問題：隨電纜長度增加，信號衰減可能影響通訊。

**建議內容：**
- 可用實際案例說明限制，例如在高頻率下的拉高電阻設計挑戰。
- 提出解決方案或最佳實踐，例如使用較短的連接線或加強 EMI 保護。

---

### **4. I2C vs Other Communication Protocols (SPI/UART)**
**重點：**
- 比較 I²C 與 SPI、UART 的優缺點：
  - **I²C**: 支援多主多從架構，硬體要求少，但速度較慢。
  - **SPI**: 支援全雙工、速度快，但需要更多的線。
  - **UART**: 使用簡單，無需時鐘線，但只支援點對點通訊。
- 實例化每個協議在不同應用場合的選擇。

**建議內容：**
- 製作比較表格，從速度、線數、拓撲結構、應用情境等維度對比三種協議。
- 引入現代應用趨勢（如 IoT）來說明各協議的適用性。

---

### **整體建議**
1. 在報告中多使用圖解（如波形圖、架構圖）以便觀眾快速理解技術細節。
2. 強調 2012 年 Ultra Fast Mode 的新增特性並與其他速度模式做比較。
3. 引入實際應用案例或常見問題（如仲裁失敗、位址衝突）增強內容實用性。
4. 各章節末加入簡要總結，幫助觀眾整理重點。


#### 資料有效性 (Data Validity)

資料有效性 指的是在 I²C 通訊中如何確保傳輸的數據準確無誤，並且能夠被接收設備正確理解。

* 數據位有效性
    * 數據位的傳輸：每個數據位的有效性是由 SCL 線的時鐘信號所同步的。只有當 SCL 線為高時，SDA 線上的數據才會被接收方讀取並判斷為有效。這一點是為了避免數據在總線上傳輸過程中的錯誤。
    * 數據位的時間要求：
        * 數據保持時間：當 SCL 線為高時，SDA 線上的數據必須保持穩定，直到 SCL 線下邊緣觸發新的位元傳輸。
* 起始與停止條件的有效性：
    * Start Condition 和 Stop Condition 是 I²C 協議中兩個關鍵的控制信號，能夠幫助識別每個傳輸的開始和結束。
    * Start Condition：當 SDA 線由高變低時，並且 SCL 線保持高，表示傳輸開始。
    * Stop Condition：當 SDA 線由低變高時，並且 SCL 線保持高，表示傳輸結束。
* 應答機制 (ACK/NACK)：
    * ACK（應答）：當從機成功接收到數據位後，會通過拉低 SDA 線來應答主機，表示該位數據已成功接收。
    * NACK（非應答）：當從機無法接收數據時，會保持 SDA 線為高，通知主機傳輸失敗。
    * ACK/NACK 的作用：這一機制保證了數據的有效性和完整性，並讓主機知道數據是否被正確接收。
* 錯誤檢測機制：
    * 時序錯誤：SDA 線的數據變化必須在 SCL 線的低邊緣進行。如果在不符合時序要求的情況下發生 SDA 線的變化，則會引發錯誤。
    * 傳輸中斷：如果在傳輸過程中出現任何異常（如 SDA 線處於不正確狀態），I²C 協議會通過 NACK 或仲裁機制來處理。
