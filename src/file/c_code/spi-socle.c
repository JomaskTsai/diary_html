/*
 * Socle SPI controller driver
 *

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 */
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/pm_runtime.h>

#include "spi-socle.h"

#define SET_TX_RX_LEN(tx, rx)	(((tx) << 16) | (rx))
#define GET_TX_LEN(len)		((len) >> 16)
#define GET_RX_LEN(len)		((len) & 0xffff)

#define SUSPND    (1<<0)
#define SPIBUSY   (1<<1)

#define MAX_CHIP_SELECT			1

struct socle_spi {
	/* bitbang has to be first */
	struct spi_bitbang bitbang;
	void __iomem	*base;
	wait_queue_head_t wq;
	struct clk *clk;
	u32 baseclk;
	int irq;
	u32 len;
	u8 bpw;
	u8 state;
#define TX_XFER_RUN 0x1
#define RX_XFER_RUN 0x2
	u32 xfer_stat;
	struct spi_master *master;
	struct spi_device *spi_dev;
	struct device *dev;
	const void *tx_buf;
	void *rx_buf;
	u32 tx_xfer_cnt;
	u32 rx_xfer_cnt;
	void (*get_rx)(struct socle_spi *soclespi);
	void (*set_tx)(struct socle_spi *soclespi);

//	struct completion	xfer_completion;
//	char spi_name[20];
};

/*
 *  Read register
 *  */
static u32 inline socle_spi_read(u32 reg, struct socle_spi *soclespi)
{
	return ioread32(soclespi->base+reg);
}

/*
 *  Write register
 *  */
static void inline socle_spi_write(u32 reg, u32 value, struct socle_spi *soclespi)
{
	iowrite32(value, soclespi->base+reg);
}

static void socle_spi_get_rx_8(struct socle_spi *soclespi)
{
	*((u8 *)soclespi->rx_buf + soclespi->rx_xfer_cnt++) = socle_spi_read(SOCLE_SPI_RXR, soclespi);
}

static void socle_spi_set_tx_8(struct socle_spi *soclespi)
{
	socle_spi_write(SOCLE_SPI_TXR, *((u8 *)soclespi->tx_buf + soclespi->tx_xfer_cnt++), soclespi);
}

static void socle_spi_get_rx_16(struct socle_spi *soclespi)
{
	*((u16 *)soclespi->rx_buf + soclespi->rx_xfer_cnt++) = socle_spi_read(SOCLE_SPI_RXR, soclespi);
}

static void socle_spi_set_tx_16(struct socle_spi *soclespi)
{
	socle_spi_write(SOCLE_SPI_TXR, *((u16 *)soclespi->tx_buf + soclespi->tx_xfer_cnt++), soclespi);
}

static u32 socle_spi_power(u32 base, u32 exp)
{
	u32 i;
	u32 value=1;

	if (exp == 0)
		return 1;
	for (i = 0; i < exp; i++)
		value *= base;
	return value;
}

/* socle_spi_calculate_divisor()
	X = Spidivr[2:0]
	Y = Spidivr[5:3]
	DivideNumber = 2^X * (Y+1)
*/
static u32  socle_spi_calculate_divisor(struct spi_device *spi, u32 clk)
{
	struct spi_master *master = spi->master;
	struct socle_spi *soclespi = spi_master_get_devdata(master);
	u8 div_high_3 = 0, div_low_3 = 0, spi_cdvr = 0;
	u32 sclk_divisor, sclk, pclk, power;

	/*APB bus clock*/
	pclk = soclespi->baseclk;

	while(1) {
		power = socle_spi_power(2, div_low_3+1);		//power = 2^div_low_3
	    	for (div_high_3 = 0; div_high_3 < 8; div_high_3++) {
	    		sclk_divisor = (div_high_3 + 1) * power;	//sclk_divisor = 2 ~ 1024
	      		sclk = pclk / sclk_divisor;			//sclk = pclk/2 ~ pclk/1024

	      		if (sclk <= clk) /* sclk <= max clock */
	      			goto out;
	    	}
		div_low_3++;	/*0~7*/
  	}

out:
	spi_cdvr = (div_high_3 << 3) | div_low_3;
	spi->max_speed_hz = sclk;
	//dev_dbg(&spi->dev, "spi clk = %d, divisor = 0x%08x,APB = 0x%08x\n", sclk, spi_cdvr, pclk);

	return spi_cdvr;
}

static void socle_spi_chipselect(struct spi_device *spi, int value)
{
	struct spi_master *master = spi->master;
	struct socle_spi *soclespi = spi_master_get_devdata(master);
	u32 slave_cs;

	slave_cs = socle_spi_read(SOCLE_SPI_FWCR, soclespi);

	if (BITBANG_CS_ACTIVE == value) {
		dev_dbg(&spi->dev, "CS_ACTIVE\n");
		slave_cs &= (~SOCLE_SPI_MASTER_SIGNAL_ACT_NO);

	} else {
		dev_dbg(&spi->dev, "CS_INACTIVE\n");
		slave_cs |= SOCLE_SPI_MASTER_SIGNAL_ACT_NO;
	}

	socle_spi_write(SOCLE_SPI_FWCR, slave_cs, soclespi);
}

static int socle_spi_setupxfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_master *master = spi->master;
	struct socle_spi *soclespi = spi_master_get_devdata(master);

	u32 speed, cpol, cpha, lsb_first, char_len, loop_back_mode, setup_val;
	u8 divisor;

	dev_dbg(soclespi->dev, "%s\n", __FUNCTION__);

	soclespi->spi_dev = spi;
	soclespi->bpw = spi->bits_per_word;

	speed = spi->max_speed_hz;

	if (spi->chip_select > (spi->master->num_chipselect - 1)) {
		dev_err(&spi->dev, "chipselect %d exceed the number of chipselect master support\n", spi->chip_select);
		return -EINVAL;
	}

	/*Bit Pre Word*/
	switch (soclespi->bpw) {
	case 4:
		char_len = SOCLE_SPI_CHAR_LEN_4;
		break;
	case 5:
		char_len = SOCLE_SPI_CHAR_LEN_5;
		break;
	case 6:
		char_len = SOCLE_SPI_CHAR_LEN_6;
		break;
	case 7:
		char_len = SOCLE_SPI_CHAR_LEN_7;
		break;
	case 0:
	case 8:
		char_len = SOCLE_SPI_CHAR_LEN_8;
		break;
	case 9:
		char_len = SOCLE_SPI_CHAR_LEN_9;
		break;
	case 10:
		char_len = SOCLE_SPI_CHAR_LEN_10;
		break;
	case 11:
		char_len = SOCLE_SPI_CHAR_LEN_11;
		break;
	case 12:
		char_len = SOCLE_SPI_CHAR_LEN_12;
		break;
	case 13:
		char_len = SOCLE_SPI_CHAR_LEN_13;
		break;
	case 14:
		char_len = SOCLE_SPI_CHAR_LEN_14;
		break;
	case 15:
		char_len = SOCLE_SPI_CHAR_LEN_15;
		break;
	case 16:
		char_len = SOCLE_SPI_CHAR_LEN_16;
		break;
	default:
		dev_err(&spi->dev, "Un-support bits per word: %d\n", soclespi->bpw);
		soclespi->bpw = 8;
		return -EINVAL;
	}

	if (SPI_CPHA & spi->mode)
		cpha = SOCLE_SPI_CPHA_1;
	else
		cpha = SOCLE_SPI_CPHA_0;

	if (SPI_CPOL & spi->mode)
		cpol = SOCLE_SPI_CPOL_1;
	else
		cpol = SOCLE_SPI_CPOL_0;

	if (SPI_LSB_FIRST & spi->mode)
		lsb_first = SOCLE_SPI_TX_LSB_FIRST;
	else
		lsb_first = SOCLE_SPI_TX_MSB_FIRST;

	if (SPI_LOOP & spi->mode)
		loop_back_mode = SOCLE_SPI_OP_LOOPBACK;
	else
		loop_back_mode = SOCLE_SPI_OP_NORMAL;

	/* In PIO mode, Set interrupt is unnecessary,enable for PIO test.
	 * we also let hardware to trigger INT, but not register ISR.
	 * We can use this INT status to monitor FIFO status.
	 */

	/* Enable SPI interrupt */
	socle_spi_write(SOCLE_SPI_IER,
			SOCLE_SPI_IER_RXFIFO_INT_EN |
			SOCLE_SPI_IER_RX_COMPLETE_INT_EN, soclespi);

	/* Enable SPI Parameter*/
	setup_val = (socle_spi_read(SOCLE_SPI_FWCR, soclespi) |
			cpol | cpha | lsb_first | loop_back_mode);
	socle_spi_write(SOCLE_SPI_FWCR, setup_val, soclespi);

	/* Configure FIFO and clear Tx & RX FIFO */
	socle_spi_write(SOCLE_SPI_FCR,
			SOCLE_SPI_RXFIFO_INT_TRIGGER_LEVEL_2 |
			SOCLE_SPI_TXFIFO_INT_TRIGGER_LEVEL_2 |
			SOCLE_SPI_RXFIFO_CLR |
			SOCLE_SPI_TXFIFO_CLR
      			, soclespi);

	/* divisor range:0~1024(2^7 * 8) */
	divisor = socle_spi_calculate_divisor(spi, speed);
	socle_spi_write(SOCLE_SPI_SSCR,
			char_len |
			SOCLE_SPI_SLAVE_SEL(spi->chip_select) |
			SOCLE_SPI_CLK_DIV(divisor), soclespi);

	/* Config SPI clock delay */
	socle_spi_write(SOCLE_SPI_DLYCR,
		SOCLE_SPI_PBTXRX_DELAY_NONE |
		SOCLE_SPI_PBCT_DELAY_NONE |
		SOCLE_SPI_PBCA_DELAY_1_2, soclespi);

	return 0;
}

static int socle_spi_setup(struct spi_device *spi)
{
	struct spi_master *master = spi->master;
	struct socle_spi *soclespi = spi_master_get_devdata(master);
	int err = 0;
	u32 spi_setting;

	dev_dbg(soclespi->dev, "%s\n", __FUNCTION__);

//	spin_lock(&soclespi->lock);

	/*SPI Master Mode setting*/
	socle_spi_write(SOCLE_SPI_FWCR,
			socle_spi_read(SOCLE_SPI_FWCR, soclespi) | SOCLE_SPI_MODE_MASTER,
			soclespi);

	/* Reset SPI Master controller */
	socle_spi_write(SOCLE_SPI_FWCR,
  		socle_spi_read(SOCLE_SPI_FWCR, soclespi) | SOCLE_SPI_MASTER_SOFT_RST,
  		soclespi);

	/*SPI Master others setting*/
	spi_setting = socle_spi_read(SOCLE_SPI_FWCR, soclespi);
	spi_setting |=	SOCLE_SPI_MASTER_SIGNAL_CTL_SW |
		   	SOCLE_SPI_MASTER_SIGNAL_ACT_NO |
		   	SOCLE_SPI_CONTROLLER_EN |
		   	SOCLE_SPI_CLK_IDLE_AST |
		   	SOCLE_SPI_TXRX_SIMULT_DIS;

	socle_spi_write(SOCLE_SPI_FWCR, spi_setting, soclespi);

//	spin_unlock(&soclespi->lock);

	return err;
}

static int socle_spi_txrx_bufs(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_master *master = spi->master;
	struct socle_spi *soclespi = spi_master_get_devdata(master);
	u32 tmp;
	u32 ret;
	int txrx_simult_en;

	soclespi->state |= SPIBUSY;	/* SPI Action status */

	/* Init and reset */
	soclespi->tx_buf = t->tx_buf;
	soclespi->rx_buf = t->rx_buf;
	soclespi->len    = t->len;
	soclespi->tx_xfer_cnt = 0;
	soclespi->rx_xfer_cnt = 0;
	soclespi->xfer_stat   = 0;	/* transfer state reset */

	if (soclespi->bpw > 8) {
		soclespi->get_rx = socle_spi_get_rx_16;
		soclespi->set_tx = socle_spi_set_tx_16;
	} else {
		soclespi->get_rx = socle_spi_get_rx_8;
		soclespi->set_tx = socle_spi_set_tx_8;
	}

	/*TX Count*/
	txrx_simult_en = 0;
	if (soclespi->tx_buf && t->len) {
		txrx_simult_en = 1;
		soclespi->xfer_stat |= TX_XFER_RUN;
		socle_spi_write(SOCLE_SPI_FCR, socle_spi_read(SOCLE_SPI_FCR, soclespi) | SOCLE_SPI_TXFIFO_CLR, soclespi);
		socle_spi_write(SOCLE_SPI_TXCR, t->len, soclespi);	/* TX set transfer count*/
		dev_dbg(&spi->dev,"TX count = %d\n",t->len);
	}

	/*RX Count*/
	if ((soclespi->rx_buf) && t->len) {
		if (!txrx_simult_en) {
			socle_spi_write(SOCLE_SPI_TXR, 0x00, soclespi);
		}/*kobe*/
		txrx_simult_en &= 1;
		soclespi->xfer_stat |= RX_XFER_RUN;
		socle_spi_write(SOCLE_SPI_RXCR, t->len, soclespi);	/* RX receive data count */
		dev_dbg(&spi->dev,"RX count = %d\n",t->len);
	}else {
		txrx_simult_en = 0;
	}

#ifdef SIMULTANEOUSLY_TRANSFER_ENABLE
	/* Simuti Setting */
	tmp = socle_spi_read(SOCLE_SPI_FWCR, soclespi);
	if (txrx_simult_en) {
		tmp |= SOCLE_SPI_TXRX_SIMULT_EN;
	} else {
		tmp &= ~SOCLE_SPI_TXRX_SIMULT_EN;
	}

	/* loopback mode disable SIMULT*/
	if (SPI_LOOP & spi->mode) {
		tmp &= ~SOCLE_SPI_TXRX_SIMULT_EN;
	}

	socle_spi_write(SOCLE_SPI_FWCR, tmp, soclespi);
#endif
	/* Write the data into tx fifo first */
	if ((soclespi->xfer_stat & TX_XFER_RUN) && soclespi->tx_buf) {
		while (SOCLE_SPI_TXFIFO_FULL != (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
			soclespi->set_tx(soclespi);
			if (soclespi->tx_xfer_cnt == soclespi->len) {
				soclespi->xfer_stat &= ~TX_XFER_RUN;
				break;
			}
		}
		/*more then TX FIFO data length use interrupt*/
		if (soclespi->xfer_stat & TX_XFER_RUN){
			socle_spi_write(SOCLE_SPI_IER,
				socle_spi_read(SOCLE_SPI_IER, soclespi) | SOCLE_SPI_IER_TXFIFO_INT_EN,
				soclespi);
		}

	}

	/* Start SPI transfer */
	socle_spi_write(SOCLE_SPI_FWCR,
		socle_spi_read(SOCLE_SPI_FWCR, soclespi) | SOCLE_SPI_TXRX_RUN,
		soclespi);

	/* Wait for Complete */
	if (soclespi->xfer_stat)
		wait_event_interruptible(soclespi->wq, soclespi->xfer_stat == 0);


	/* Wait for the transaction to be complete */
	do {
		tmp = socle_spi_read(SOCLE_SPI_FWCR, soclespi);
	} while ((tmp & SOCLE_SPI_TXRX_RUN) == SOCLE_SPI_TXRX_RUN);

	if (soclespi->tx_buf && soclespi->rx_buf) {
		t->len = soclespi->rx_xfer_cnt;
		ret = soclespi->rx_xfer_cnt;
	} else if (soclespi->tx_buf) {
		t->len = soclespi->tx_xfer_cnt;
		ret = soclespi->tx_xfer_cnt;
	} else {
		t->len = soclespi->rx_xfer_cnt;
		ret = soclespi->rx_xfer_cnt;
	}

	soclespi->state &= ~SPIBUSY;

	return ret;
}

static int socle_spi_transfer_one_message(struct spi_master *master,
			struct spi_message *msg)
{
	//struct socle_spi *soclespi = spi_master_get_devdata(master);
	struct spi_device *spi = msg->spi;
	struct spi_transfer *t = NULL;
	int status = 0;
	unsigned int cs_change = 1;

	list_for_each_entry(t, &msg->transfers, transfer_list) {

		if (cs_change) {
			status = socle_spi_setupxfer(spi, t);
			if (status)
				break;
			socle_spi_chipselect(spi, BITBANG_CS_ACTIVE);
		}
		/*get SPI transfer cs_change*/
		cs_change = t->cs_change;

		status = socle_spi_txrx_bufs(spi, t);
		if (status < 0)
			break;

		status = 0;

		if (t->delay_usecs)
			udelay(t->delay_usecs);

		/* SPI Transfer do Chip Select Change */
		if (cs_change && !list_is_last(&t->transfer_list, &msg->transfers)) {
				socle_spi_chipselect(spi, BITBANG_CS_INACTIVE);
		}

		msg->actual_length += t->len;
	}

	/*message finished*/
	if (!(status == 0 && cs_change)) {
		socle_spi_chipselect(spi, BITBANG_CS_INACTIVE);
	}

	msg->status = status;
	spi_finalize_current_message(master);

	return status;
}
static int soclespi_prepare_transfer_hw(struct spi_master *master)
{
	struct socle_spi *soclespi = spi_master_get_devdata(master);

	pm_runtime_get_sync(soclespi->dev);

	return 0;
}

static int soclespi_unprepare_transfer_hw(struct spi_master *master)
{
	struct socle_spi *soclespi = spi_master_get_devdata(master);

	pm_runtime_put_sync(soclespi->dev);

	return 0;
}

static irqreturn_t socle_spi_isr(int irq, void *_host)
{
	struct socle_spi *soclespi = (struct socle_spi *)_host;
	u32 status;

	/* Read & clear interrupt status */
	status = socle_spi_read(SOCLE_SPI_ISR, soclespi);

	/* Check if the receive data is overrun */
	if (SOCLE_SPI_RXFIFO_OVR_INT == (status & SOCLE_SPI_RXFIFO_OVR_INT)) {
		dev_dbg(soclespi->dev, "RX FIFO overrun\n");
		dev_err(soclespi->dev, "receive FIFO is full and another character has been received in the receiver shift register\n");
		if (soclespi->tx_buf)
			soclespi->xfer_stat &= ~TX_XFER_RUN;

		if (soclespi->rx_buf)
			soclespi->xfer_stat &= ~RX_XFER_RUN;

		wake_up_interruptible(&soclespi->wq);
		goto out;
	}

	/* Check is RX complete */
	if (SOCLE_SPI_RX_COMPLETE_INT == (status & SOCLE_SPI_RX_COMPLETE_INT)) {
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			soclespi->get_rx(soclespi);
		}
		soclespi->xfer_stat &= ~RX_XFER_RUN;
		wake_up_interruptible(&soclespi->wq);
		goto _TX;
	}

	/* Check if any RX is available */
	if (SOCLE_SPI_RXFIFO_INT == (status & SOCLE_SPI_RXFIFO_INT)) {
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			soclespi->get_rx(soclespi);
		}
	}
_TX:
	/* Check if the TX is available */
	if (SOCLE_SPI_TXFIFO_INT == (status & SOCLE_SPI_TXFIFO_INT)) {
		while (SOCLE_SPI_TXFIFO_FULL != (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
			soclespi->set_tx(soclespi);
			if (soclespi->tx_xfer_cnt == soclespi->len) {/* tx transfer done */
				socle_spi_write(SOCLE_SPI_IER,
					socle_spi_read(SOCLE_SPI_IER, soclespi) & ~SOCLE_SPI_IER_TXFIFO_INT_EN,
					soclespi);
				soclespi->xfer_stat &= ~TX_XFER_RUN;
				wake_up_interruptible(&soclespi->wq);
				break;
			}
		}
	}
out:
	return IRQ_HANDLED;
}

static const struct of_device_id socle_spi_of_match[] = {
	{
		.compatible = "socle,general-spi",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, socle_spi_of_match);

static int socle_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct socle_spi *soclespi;
	struct resource	*r;
	int status = 0;
	//static int bus_num = 0;
	const struct of_device_id *match;

	//struct device_node *node = pdev->dev.of_node;

	dev_dbg(&pdev->dev, "%s\n", __FUNCTION__);

	/* Get Platform data */
	match = of_match_device(socle_spi_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	//of_property_read_u32(node, "num-cs", &num_cs);

	/* Create SPI Master */
	master = spi_alloc_master(&pdev->dev, sizeof *soclespi);
	if (master == NULL) {
		dev_dbg(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST | SPI_LOOP;
	master->setup = socle_spi_setup;
	master->transfer_one_message = socle_spi_transfer_one_message;
	master->prepare_transfer_hardware = soclespi_prepare_transfer_hw;
	master->unprepare_transfer_hardware = soclespi_unprepare_transfer_hw;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = 2; /*need verify*/

	dev_set_drvdata(&pdev->dev, master);

	soclespi = spi_master_get_devdata(master);
	soclespi->master = master;
	soclespi->dev = &pdev->dev;
	//spin_lock_init(&soclespi->lock);


	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	soclespi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(soclespi->base)) {
		status = PTR_ERR(soclespi->base);
		goto exit_free_master;
	}
	/*Get SPI IRQ*/
	soclespi->irq = platform_get_irq(pdev, 0);
	//snprintf(soclespi->spi_name, sizeof(soclespi->spi_name), "%s %d",
	//	pdev->name, soclespi->master->bus_num);
	
	/* Allocate the interrupt */
	//status = request_irq(soclespi->irq, socle_spi_isr, IRQF_DISABLED, dev_name(&pdev->dev), soclespi);
	status = devm_request_irq(&pdev->dev, soclespi->irq, socle_spi_isr, 0, dev_name(&pdev->dev), soclespi);
	if (status) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
					soclespi->irq);
		goto exit_free_master;
	}

	init_waitqueue_head(&soclespi->wq);

	/* Get SPI clk */
	soclespi->clk = devm_clk_get(&pdev->dev, "spi_mclk");
	if (IS_ERR(soclespi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		goto exit_free_master;
	}
	clk_prepare_enable(soclespi->clk);
	soclespi->baseclk = clk_get_rate(soclespi->clk);
	dev_dbg(&pdev->dev,"SPI CLK=0x%x\n",soclespi->baseclk);

//	init_completion(&soclespi->xfer_completion);

	master->dev.of_node = pdev->dev.of_node;
	status = spi_register_master(master);
	if (status < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", status);
		goto exit_free_master;
	}
	dev_dbg(&pdev->dev,"SPI Probe Done\n");
	return status;


exit_free_master:
	spi_master_put(master);

	return status;
}


static int socle_spi_remove(struct platform_device *pdev)
{
	struct socle_spi *soclespi = platform_get_drvdata(pdev);

	spi_bitbang_stop(&soclespi->bitbang);
	platform_set_drvdata(pdev, NULL);
	free_irq(soclespi->irq, soclespi);
	spi_master_put(soclespi->master);
	dev_dbg(soclespi->dev, "socle_spi_remove()\n");

	return 0;
}

#ifdef CONFIG_PM
static int socle_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct socle_spi *soclespi = platform_get_drvdata(pdev);

	while (soclespi->state & SPIBUSY)
		msleep(10);

	/* disable the spi master */
	socle_spi_write(SOCLE_SPI_FWCR,
		socle_spi_read(SOCLE_SPI_FWCR, soclespi) & ~SOCLE_SPI_CONTROLLER_EN
		, soclespi);

	return 0;
}

static int socle_spi_resume(struct platform_device *pdev)
{
	struct socle_spi *soclespi = platform_get_drvdata(pdev);
	struct spi_device *spi = soclespi->spi_dev;
	int err;

	/* Reset SPI controller */
	socle_spi_write(SOCLE_SPI_FWCR,
  		socle_spi_read(SOCLE_SPI_FWCR, soclespi) |SOCLE_SPI_MASTER_SOFT_RST,
  		soclespi);

	/* if this spi master haved setuped, setup again at resume */
	err = spi->master->setup(spi);
	if (err)
		dev_err(&spi->dev, "setup_transfer returned %d\n", err);

	return 0;
}
#else
#define socle_spi_suspend NULL
#define socle_spi_resume NULL
#endif

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:Socle-SPI");

static struct platform_driver SOCLE_SPI_driver = {
	.driver = {
		.name =	"Socle-SPI",
		.owner = THIS_MODULE,
		.of_match_table = socle_spi_of_match,
	},
	.probe = socle_spi_probe,
	.remove = socle_spi_remove,
	.suspend = socle_spi_suspend,
	.resume = socle_spi_resume,
};

module_platform_driver(SOCLE_SPI_driver);
MODULE_DESCRIPTION("Socle SPI Master Driver");
MODULE_LICENSE("GPL");
