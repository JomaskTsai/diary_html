---
title: "I2C Protocol Presentation"
date: 2024-10-03T16:13:37+08:00
draft: false
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

