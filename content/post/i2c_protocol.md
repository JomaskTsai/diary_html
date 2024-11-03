---
title: "I2C Protocol Presentation"
date: 2024-10-03T16:13:37+08:00
draft: true
---

# I2C Intoduction
## I2C Inter-Integrated Circuit
I²C（Inter-Integrated Circuit）是一種由飛利浦半導體（現為恩智浦半導體 NXP Semiconductors）於1982 年發布的串行通訊協議。I²C協議的主要制定者是飛利浦，目前則由恩智浦負責維護。該協議最初設計的目的是讓微控制器（MCU）可以簡單且低成本地與其他元件（如傳感器、顯示器和存儲設備等）進行通信。

I²C 使用兩條信號線——數據線（SDA）和時鐘線（SCL）來傳輸數據，具有同步且雙向的特性。I²C 可以支持多個主設備與從設備之間的通訊，因此適用於多數嵌入式系統和消費性電子產品中，特別是在需要多元件之間低速通訊的場景。I2C 用於各種控制架構，例如係統管理匯流排（SMBus）、電源管理匯流排 (PMBus)、智慧型平台管理介面 (IPMI)、顯示器資料通道 (DDC) 和高階電信運算架構 (ATCA) 

![I2C Version History](/images/figure_1.png)

<table>
    <tr>
        <td>Version</td>
        <td>Describtion</td>
    </tr>
    <tr>
        <td>First</td>
        <td>The 1st version of I2C protocol.</td>
    </tr>
    <tr>
        <td>1.0</td>
        <td>N/A</td>
    </tr>
        <tr>
        <td>2.0</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>2.1</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>3.0</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>4.0</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>5.0</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>6.0</td>
        <td>N/A</td>
    </tr>
    </tr>
        <tr>
        <td>7.0</td>
        <td>N/A</td>
    </tr>
</table>

# Test  2



# Agenda

1. Introduction to I2C Protocol
2. I2C Protocol Overview (Version 2.0)
3. I2C Physical Layer
4. Master-Slaver Architecture
5. Addressing Scheme
6. Data Transfer Format
7. Transmission Speeds
8. Start and Stop Condition
9. Repeated Start Condition
10. Acknowledgement Mechanism (ACK/NACK)

---

11. Clocl Stretching
12. Bus Arbitration
13. Multi-Master Configuration
14. Error Detection and Handling
15. Voltage Levels and Pull-up Resistors
16. I2C vs Other Communication Protocols (SPI/UART)
17. I2C Protocol Challenges and Limitations

