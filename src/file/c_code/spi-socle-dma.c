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
#include <linux/dma-mapping.h>

#include <linux/dmaengine.h>

#include <linux/err.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include "spi-socle.h"



#define SET_TX_RX_LEN(tx, rx)	(((tx) << 16) | (rx))
#define GET_TX_LEN(len)		((len) >> 16)
#define GET_RX_LEN(len)		((len) & 0xffff)

#define SUSPND    (1<<0)
#define SPIBUSY   (1<<1)

struct socle_spi_platform_data {
	u8 master_enable;
	u8 wifi_module_use;
};

/**
 * struct socle_dma_slave - Controller-specific information about a slave
 * @dma_dev: required DMA master device
 * @cfg: Platform-specific initializer for the CFG register
 */
struct socle_dma_slave {
	struct device		*dma_dev;
	u32			cfg;
};

//struct socle_spi_dma {
//	struct dma_chan			*chan_rx;
//	struct dma_chan			*chan_tx;
//	struct scatterlist		sgrx;
//	struct scatterlist		sgtx;
//	struct dma_async_tx_descriptor	*desc_rx;
//	struct dma_async_tx_descriptor	*desc_tx;
//
//	struct socle_dma_slave	dma_slave;
//};

struct socle_spi_host {
	/* bitbang has to be first */
	struct spi_bitbang bitbang;
	void __iomem	*base;
	wait_queue_head_t wq;
	int irq;
	u32 len;
	u8 bpw;
	struct clk *clk;
	u32 baseclk;
	u32 tx_xfer_cnt;
	u32 rx_xfer_cnt;
	u32 tx_total_cnt;
	u32 rx_total_cnt;
	
	u32 xfer_stat;
	u8 master_setup;
	u8 state;
#define TX_XFER_DONE 0x1
#define RX_XFER_DONE 0x2
	struct spi_master *master;
	struct spi_device *spi_dev;
	struct device *dev;
	const void *tx_buf;
	void *rx_buf;
	void (*get_rx)(struct socle_spi_host *soclespi);
	void (*set_tx)(struct socle_spi_host *soclespi);
	void (*dma_get_last_rx)(struct socle_spi_host *soclespi);
	void (*dma_set_last_tx)(struct socle_spi_host *soclespi);
	char spi_name[20];
	
	
	dma_addr_t	dma_tx_buf;
	dma_addr_t	dma_rx_buf;
	u32 dma_tx_count;
	u32 dma_rx_count;
	u8 dma_tx_avail;
	u8 dma_rx_avail;
	u32 dma_tx_buffer_size;
	u32 dma_rx_buffer_size;
	u32 spi_fifo_addr;
	
	u32				dma_tx_slave_id;
	u32				dma_rx_slave_id;
			
	struct dma_chan			*dma_tx_chan;
	struct dma_chan			*dma_rx_chan;
	struct scatterlist		dma_tx_sg;
	struct scatterlist		dma_rx_sg;
	struct dma_async_tx_descriptor	*dma_desc_tx;
	struct dma_async_tx_descriptor	*dma_desc_rx;
	
//	struct socle_spi_dma_info *spi_dma_info;
};


const struct socle_spi_platform_data *socle_spi_info=NULL;

/*
 *  Read register
 *  */
static u32 inline socle_spi_read(u32 reg, struct socle_spi_host *soclespi)
{
	return ioread32(soclespi->base+reg);
}
/*static u32 inline socle_spi_read(u32 reg, void __iomem *base)
{
	return ioread32(base+reg);
}*/

/*
 *  Write register
 *  */
static void inline socle_spi_write(u32 reg, u32 value, struct socle_spi_host *soclespi)
{
	iowrite32(value, soclespi->base+reg);
}
/*static void inline socle_spi_write(u32 reg, u32 value, void __iomem *base)
{
	iowrite32(value, base+reg);
}*/

static void socle_spi_get_rx_8(struct socle_spi_host *soclespi)
{
	//dev_dbg(soclespi->dev, "socle_spi_get_rx_8()\n");
	*((u8 *)soclespi->rx_buf + soclespi->rx_xfer_cnt++) = socle_spi_read(SOCLE_SPI_RXR, soclespi);
}

static void socle_spi_set_tx_8(struct socle_spi_host *soclespi)
{
	//dev_dbg(soclespi->dev, "socle_spi_set_tx_8()\n");
	socle_spi_write(SOCLE_SPI_TXR, *((u8 *)soclespi->tx_buf + soclespi->tx_xfer_cnt++), soclespi);
}

static void socle_spi_get_rx_16(struct socle_spi_host *soclespi)
{
	//dev_dbg(soclespi->dev, "socle_spi_set_rx_16()\n");
	*((u16 *)soclespi->rx_buf + soclespi->rx_xfer_cnt++) = socle_spi_read(SOCLE_SPI_RXR, soclespi);
}

static void socle_spi_set_tx_16(struct socle_spi_host *soclespi)
{
	//dev_dbg(soclespi->dev, "socle_spi_get_tx_16()\n");
	socle_spi_write(SOCLE_SPI_TXR, *((u16 *)soclespi->tx_buf + soclespi->tx_xfer_cnt++), soclespi);
}

static void socle_spi_dma_get_last_rx_8(struct socle_spi_host *soclespi)
{
	u8 *rx_buf_temp;
	u8 rx_last_count = 0;

	rx_buf_temp = (u8 *)soclespi->rx_buf;
	rx_buf_temp += soclespi->dma_rx_count;

	if (soclespi->dma_rx_avail !=0) {
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == \
			(socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			*rx_buf_temp = socle_spi_read(SOCLE_SPI_RXR, soclespi);
			rx_buf_temp++;		//next rx addr
			rx_last_count++;	//rx count
			if(rx_last_count == soclespi->dma_rx_avail)
				break;
		}
	}
}

static void socle_spi_dma_set_last_tx_8(struct socle_spi_host *soclespi)
{
	u8 *tx_buf_temp;
	u8 tx_last_count = 0;

	tx_buf_temp = (u8 *)soclespi->tx_buf;
	tx_buf_temp += soclespi->dma_tx_count;

	if(soclespi->dma_tx_avail != 0) {
		while (SOCLE_SPI_TXFIFO_FULL != \
			(socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
			socle_spi_write(SOCLE_SPI_TXR, *tx_buf_temp, soclespi);
			tx_buf_temp++;
			tx_last_count++;
			if(tx_last_count == soclespi->dma_tx_avail)
				break;
		}
	}
}

static void socle_spi_dma_get_last_rx_16(struct socle_spi_host *soclespi)
{
	u16 *rx_buf_temp;
	u8 rx_last_count=0;;

	rx_buf_temp = (u16 *)soclespi->rx_buf;
	rx_buf_temp += soclespi->dma_rx_count;

	if (soclespi->dma_rx_avail !=0) {
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == \
			(socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			*rx_buf_temp = socle_spi_read(SOCLE_SPI_RXR, soclespi);
			rx_buf_temp++;
			rx_last_count++;
			if(rx_last_count == soclespi->dma_rx_avail)
				break;
		}
	}
}

static void socle_spi_dma_set_last_tx_16(struct socle_spi_host *soclespi)
{
	u16 *tx_buf_temp;
	u8 tx_last_count = 0;

	tx_buf_temp = (u16 *)soclespi->tx_buf;
	tx_buf_temp += soclespi->dma_tx_count;

	if(soclespi->dma_tx_avail != 0) {
		while (SOCLE_SPI_TXFIFO_FULL != \
			(socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
			socle_spi_write(SOCLE_SPI_TXR, *tx_buf_temp, soclespi);
			tx_buf_temp++;
			tx_last_count++;
			if(tx_last_count == soclespi->dma_tx_avail)
				break;
		}
	}
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

static u32  socle_spi_calculate_divisor(struct spi_device *spi, u32 clk)
{
	struct spi_master *master = spi->master;
	struct socle_spi_host *soclespi = spi_master_get_devdata(master);
	u8 div_high_3 = 0, div_low_3 = 0, spi_cdvr = 0;
	u32 sclk_divisor, sclk, pclk, power;

	pclk = soclespi->baseclk;	/*bus clock*/
	while(1){
		power = socle_spi_power(2, div_low_3+1);		//power =2,4,8
	    	for (div_high_3 = 0; div_high_3 < 8; div_high_3++) {
	    		sclk_divisor = (div_high_3 + 1) * power;	//sclk_divisor=2,4,6...16
	    								//	       4,8,12..32
	    								//	       8,16,24.64
	      		sclk = pclk / sclk_divisor;

	      		if (sclk <= clk) /*sclk <= max clock*/
	      			goto out;
	    	}
		div_low_3++;	/*0~7*/
  	}

out:
	spi_cdvr = (div_high_3 << 3) | div_low_3;
//	dev_dbg(&spi->dev, "sclk is %d, divisor is 0x%08x,APB is 0x%08x\n", sclk, spi_cdvr, pclk);
	spi->max_speed_hz = sclk;
	
	return spi_cdvr;
}

static void socle_spi_chipselect(struct spi_device *spi, int value)
{
	struct spi_master *master = spi->master;
	struct socle_spi_host *soclespi = spi_master_get_devdata(master);

//	dev_dbg(soclespi->dev, "socle_spi_chipselect()\n");

	if (BITBANG_CS_ACTIVE == value) {
		dev_dbg(&spi->dev, "BITBANG_CS_ACTIVE\n");

	} else {
		dev_dbg(&spi->dev, "BITBANG_CS_INACTIVE\n");
	}
}

static int socle_spi_setupxfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_master *master = spi->master;
	struct socle_spi_host *soclespi = spi_master_get_devdata(master);
	
	u32 speed, cpol, cpha, lsb_first, char_len,loop_back_mode;
	u8 divisor;

	//dev_dbg(soclespi->dev, "socle_spi_setupxfer()\n");

	soclespi->spi_dev = spi;
	soclespi->master_setup = 1;
	
	soclespi->bpw = spi->bits_per_word;
	speed 	      = spi->max_speed_hz;

	if (spi->chip_select > (spi->master->num_chipselect - 1)) {
		dev_err(&spi->dev, "chipselect %d exceed the number of chipselect master supoort\n", spi->chip_select);
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
		printk("\n---------------len = 8--------------\n");
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

	/* Configure SPI controller for master*/
	if (socle_spi_info->master_enable)
		socle_spi_write(SOCLE_SPI_FWCR,
				SOCLE_SPI_MASTER_SIGNAL_CTL_HW |
				SOCLE_SPI_MASTER_SIGNAL_ACT_NO |
				SOCLE_SPI_MODE_MASTER |
				SOCLE_SPI_MASTER_SOFT_N_RST |
				SOCLE_SPI_CONTROLLER_EN |
				SOCLE_SPI_TXRX_N_RUN |
				SOCLE_SPI_CLK_IDLE_AST |
				SOCLE_SPI_TXRX_SIMULT_DIS |
				cpol |
				cpha |
				lsb_first |
				SOCLE_SPI_DMA_REQ |
				SOCLE_SPI_MODE_UNI_DIR |
				loop_back_mode, soclespi);
	else
		socle_spi_write(SOCLE_SPI_FWCR,
				SOCLE_SPI_MASTER_SOFT_N_RST |
				SOCLE_SPI_MODE_SLAVE |
				SOCLE_SPI_CONTROLLER_EN |
				SOCLE_SPI_TXRX_N_RUN |
				SOCLE_SPI_CLK_IDLE_AST |
				SOCLE_SPI_TXRX_SIMULT_DIS |
				cpol |
				cpha |
				lsb_first |
				SOCLE_SPI_DMA_REQ |
				SOCLE_SPI_MODE_UNI_DIR |
				SOCLE_SPI_OP_NORMAL, soclespi);


	/* Configure FIFO and clear Tx & RX FIFO */
	socle_spi_write(SOCLE_SPI_FCR,
			SOCLE_SPI_RXFIFO_INT_TRIGGER_LEVEL_4 |
			SOCLE_SPI_TXFIFO_INT_TRIGGER_LEVEL_2 |
			SOCLE_SPI_RXFIFO_CLR |
			SOCLE_SPI_TXFIFO_CLR
      			, soclespi);

	/* Set the SPI slaves select and characteristic control register */
	/*divisor range:0~64*/
	divisor = socle_spi_calculate_divisor(spi, speed);

	socle_spi_write(SOCLE_SPI_SSCR,
			char_len |
			SOCLE_SPI_SLAVE_SEL(spi->chip_select) |
			SOCLE_SPI_CLK_DIV(divisor), soclespi);

	/* Config SPI clock delay */
	if (socle_spi_info->wifi_module_use)
		socle_spi_write(SOCLE_SPI_DLYCR,
			SOCLE_SPI_PBTXRX_DELAY_32 |
			SOCLE_SPI_PBCT_DELAY_NONE |
			SOCLE_SPI_PBCA_DELAY_1_2, soclespi);
	else
		socle_spi_write(SOCLE_SPI_DLYCR,
			SOCLE_SPI_PBTXRX_DELAY_NONE |
			SOCLE_SPI_PBCT_DELAY_NONE |
			SOCLE_SPI_PBCA_DELAY_1_2, soclespi);

	return 0;
}

static int socle_spi_setup(struct spi_device *spi)
{
	struct spi_master *master = spi->master;
	struct socle_spi_host *soclespi = spi_master_get_devdata(master);
	int err = 0;

	//dev_dbg(soclespi->dev, "socle_spi_setup()\n");
		
	err = socle_spi_setupxfer(spi, NULL);
	if (err)
		dev_err(&spi->dev, "setup_transfer returned %d\n", err);
//	dev_dbg(&spi->dev, "%s: mode %d, %u bpw, %d speed\n", __FUNCTION__, spi->mode, spi->bits_per_word, spi->max_speed_hz);
	return err;
}

static void socle_spi_dma_tx_callback(void *data)
{
	struct socle_spi_host *soclespi = (struct socle_spi_host *)data;
	
	soclespi->xfer_stat |= TX_XFER_DONE;
	
	if (soclespi->dma_tx_buf )
		dma_unmap_single(soclespi->dev, soclespi->dma_tx_buf, soclespi->dma_tx_count, DMA_TO_DEVICE);
	
	soclespi->dma_set_last_tx(soclespi);
	wake_up_interruptible(&soclespi->wq);
}

static void socle_spi_dma_rx_callback(void *data)
{
	struct socle_spi_host *soclespi = (struct socle_spi_host *)data;

	soclespi->xfer_stat |= RX_XFER_DONE;
	if (soclespi->dma_rx_buf )
		dma_unmap_single(soclespi->dev, soclespi->dma_rx_buf, soclespi->dma_rx_count, DMA_FROM_DEVICE);
	
	soclespi->dma_get_last_rx(soclespi);
	wake_up_interruptible(&soclespi->wq);
}

static int socle_spi_dma_tx_slave_config(struct socle_spi_host *soclespi,u8 bits_per_word)
{
	int err = 0;
	struct scatterlist sg_tx;
	struct dma_slave_config slave_config;
	struct dma_async_tx_descriptor *dma_tx_desc;
	
	if (bits_per_word > 8) {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	}

	slave_config.dst_addr = (dma_addr_t)soclespi->spi_fifo_addr;
	slave_config.dst_maxburst = 1;
	slave_config.slave_id = soclespi->dma_tx_slave_id;
	slave_config.direction = DMA_MEM_TO_DEV;
	if (dmaengine_slave_config(soclespi->dma_tx_chan , &slave_config)) {
		dev_err(soclespi->dev,
			"failed to configure tx dma channel\n");
		err = -EINVAL;
	}

	sg_init_table(&sg_tx, 1);
			
	/* DMA TX Buffer */			
	soclespi->dma_tx_buf = dma_map_single(soclespi->dev,
		(void *) soclespi->tx_buf, soclespi->dma_tx_count,
		DMA_TO_DEVICE);
	if (dma_mapping_error(soclespi->dev, soclespi->dma_tx_buf)){
		printk("spi debug dma_mapping_error\n");
		return -ENOMEM;
	}
				
	/* DMA gs_tx */
	sg_dma_address(&sg_tx) = soclespi->dma_tx_buf;
	sg_dma_len(&sg_tx)     = soclespi->dma_tx_count;

	/* DMA TX Desc */
	dma_tx_desc = dmaengine_prep_slave_sg(soclespi->dma_tx_chan,
			&sg_tx, 1, DMA_MEM_TO_DEV,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_tx_desc)
		goto err_tx_desc;

	dma_tx_desc->callback = socle_spi_dma_tx_callback;
	dma_tx_desc->callback_param = (void *)soclespi;

	dmaengine_submit(dma_tx_desc);
	dma_async_issue_pending(soclespi->dma_tx_chan);
	return err;
	
err_tx_desc:
	dma_unmap_single(soclespi->dev, soclespi->dma_tx_buf, soclespi->dma_tx_count, DMA_TO_DEVICE);
	printk("spi DMA debug err_desc:\n");
	return -EINVAL;;
}

static int socle_spi_dma_rx_slave_config(struct socle_spi_host *soclespi,u8 bits_per_word)
{
	int err = 0;
	struct scatterlist sg_rx;
	struct dma_slave_config slave_config;
	struct dma_async_tx_descriptor *dma_rx_desc;
	
	if (bits_per_word > 8) {
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else {
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	}

	slave_config.src_addr = (dma_addr_t)soclespi->spi_fifo_addr;
	slave_config.src_maxburst = 1;
	slave_config.slave_id = soclespi->dma_rx_slave_id;
	
	slave_config.direction = DMA_DEV_TO_MEM;
	if (dmaengine_slave_config(soclespi->dma_rx_chan , &slave_config)) {
		dev_err(soclespi->dev,
			"failed to configure rx dma channel\n");
		err = -EINVAL;
	}

	sg_init_table(&sg_rx, 1);
			
	/* DMA TX Buffer */			
	soclespi->dma_rx_buf = dma_map_single(soclespi->dev,
		(void *) soclespi->rx_buf, soclespi->dma_rx_count,
		DMA_FROM_DEVICE);
	if (dma_mapping_error(soclespi->dev, soclespi->dma_rx_buf)){
		printk("spi DMA debug dma_mapping_error\n");
		return -ENOMEM;
	}
				
	/* DMA gs_rx */
	sg_dma_address(&sg_rx) = soclespi->dma_rx_buf;
	sg_dma_len(&sg_rx)     = soclespi->dma_rx_count;

	/* DMA RX Desc */
	dma_rx_desc = dmaengine_prep_slave_sg(soclespi->dma_rx_chan,
			&sg_rx, 1, DMA_DEV_TO_MEM,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_rx_desc)
		goto err_tx_desc;

	dma_rx_desc->callback = socle_spi_dma_rx_callback;
	dma_rx_desc->callback_param = (void *)soclespi;

	dmaengine_submit(dma_rx_desc);
	dma_async_issue_pending(soclespi->dma_rx_chan);
	return err;

err_tx_desc:
	dma_unmap_single(soclespi->dev, soclespi->dma_rx_buf, soclespi->dma_rx_count, DMA_FROM_DEVICE);
	printk("spi DMA debug err_tx_desc:\n");
	return -EINVAL;;
}

static int socle_spi_txrx_bufs(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_master *master = spi->master;
	struct socle_spi_host *soclespi = spi_master_get_devdata(master);
	u32 tmp;
	u32 ret;
	u8 xfer_complete = 0;
	int txrx_simult_en;
	
	dev_dbg(soclespi->dev, "socle_spi_txrx_bufs()\n");
	
	soclespi->state |= SPIBUSY;	/* SPI Action status */
	
	/* Init and reset */	
	soclespi->tx_buf = t->tx_buf;
	soclespi->rx_buf = t->rx_buf;

	soclespi->tx_total_cnt = GET_TX_LEN(t->len);
	soclespi->rx_total_cnt = GET_RX_LEN(t->len);
	
	soclespi->tx_xfer_cnt = 0;
	soclespi->rx_xfer_cnt = 0;
	soclespi->xfer_stat   = 0;	/* transfer state reset */
	
	if (soclespi->bpw > 8) {
		soclespi->get_rx = socle_spi_get_rx_16;
		soclespi->set_tx = socle_spi_set_tx_16;
		soclespi->dma_get_last_rx = socle_spi_dma_get_last_rx_16;
		soclespi->dma_set_last_tx = socle_spi_dma_set_last_tx_16;
	} else {
		soclespi->get_rx = socle_spi_get_rx_8;
		soclespi->set_tx = socle_spi_set_tx_8;
		soclespi->dma_get_last_rx = socle_spi_dma_get_last_rx_8;
		soclespi->dma_set_last_tx = socle_spi_dma_set_last_tx_8;	
	}

	/*TX Count setting*/
	txrx_simult_en = 0;
	if (soclespi->tx_buf && (soclespi->tx_total_cnt != 0)) {
		xfer_complete |=TX_XFER_DONE;				
		txrx_simult_en = 1;
	}
	socle_spi_write(SOCLE_SPI_TXCR, soclespi->tx_total_cnt, soclespi);	/* TX set transfer count*/
	printk("spi DMA debug Tx count=%d\n",soclespi->tx_total_cnt);	
	
	/*RX Count setting*/
	if ((soclespi->rx_buf) && (soclespi->rx_total_cnt != 0)) {
		xfer_complete |=RX_XFER_DONE;	
		txrx_simult_en &= 1;
	}else{
		txrx_simult_en = 0;
	}
	socle_spi_write(SOCLE_SPI_RXCR, soclespi->rx_total_cnt, soclespi);	/* RX receive data count */
	printk("spi DMA debug Rx count=%d\n",soclespi->rx_total_cnt);
	
	/* Simuti Setting */
	tmp = socle_spi_read(SOCLE_SPI_FWCR, soclespi);
	if(txrx_simult_en){
		tmp |= SOCLE_SPI_TXRX_SIMULT_EN;
	}else{
		tmp &= ~SOCLE_SPI_TXRX_SIMULT_EN;
	}
	/* loopback mode disable SIMULT*/
	if (SPI_LOOP & spi->mode){
		tmp &= ~SOCLE_SPI_TXRX_SIMULT_EN;
	}	
	socle_spi_write(SOCLE_SPI_FWCR, tmp, soclespi);
			
	socle_spi_write(SOCLE_SPI_FWCR,
		socle_spi_read(SOCLE_SPI_FWCR, soclespi) | SOCLE_SPI_TXRX_RUN, 
		soclespi);
	if (soclespi->tx_buf) {
			soclespi->dma_tx_count = soclespi->tx_total_cnt;//kobecheng
			ret = socle_spi_dma_tx_slave_config(soclespi, soclespi->bpw);
			if(ret){
				return ret;
			}
/*		if (soclespi->tx_total_cnt > 8) {
*/			/* 4 byte align valid */
/*			soclespi->dma_tx_avail = soclespi->tx_total_cnt %4;			
			if (soclespi->dma_tx_avail != 0)
				soclespi->dma_tx_count = soclespi->tx_total_cnt - soclespi->dma_tx_avail;
			else
				soclespi->dma_tx_count = soclespi->tx_total_cnt;
			
			printk("dma_tx_avail = %d\n", soclespi->dma_tx_avail);
			printk("dma_tx_count = %d\n", soclespi->dma_tx_count);
*/			/* TX DMA Setting*/
/*			ret = socle_spi_dma_tx_slave_config(soclespi, soclespi->bpw);
			if(ret){
				return ret;
			}
		} else {
 */ /* TX CPU Mode */		
/*			while (SOCLE_SPI_TXFIFO_FULL != (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
//				if (soclespi->tx_xfer_cnt == soclespi->len) {
				if (soclespi->tx_xfer_cnt == soclespi->tx_total_cnt) {
					soclespi->xfer_stat |= TX_XFER_DONE;
					break;
				}
				soclespi->set_tx(soclespi);
			}
*/			/* below is need ?? (TX FIFO FULL When TX size<=8)*/
/*			if(soclespi->xfer_stat != TX_XFER_DONE){
				socle_spi_write(SOCLE_SPI_IER, 
					socle_spi_read(SOCLE_SPI_IER, soclespi) | 
					SOCLE_SPI_IER_TXFIFO_INT_EN, 
					soclespi);
			}	
		}*/	/*kobecheng*/
	}
///////////////spi dma_request = trigger level, single read only one, but until the fifo empty//kobecheng
	if (soclespi->rx_buf) {
		if(soclespi->rx_total_cnt > 3/*trigger level minus 1 //kobecheng */) {
			soclespi->dma_rx_avail = 3;//trigger level minus 1 //kobecheng
			soclespi->dma_rx_count = soclespi->rx_total_cnt - soclespi->dma_rx_avail;
/*			soclespi->dma_rx_avail = soclespi->rx_total_cnt%4;
				
			if (soclespi->dma_rx_avail != 0)
				soclespi->dma_rx_count = soclespi->rx_total_cnt - soclespi->dma_rx_avail;
			else
				soclespi->dma_rx_count = soclespi->rx_total_cnt;
*/				
		//	printk("dma_rx_avail = %d\n", soclespi->dma_rx_avail);
		//	printk("dma_rx_count = %d\n", soclespi->dma_rx_count);

			/* RX DMA Setting*/
			ret = socle_spi_dma_rx_slave_config(soclespi, soclespi->bpw);
			if(ret){
				return ret;
			}
			
		}else {
			/* Enable SPI interrupt */
		//	printk("enable spi interrupt");
			socle_spi_write(SOCLE_SPI_IER,
					socle_spi_read(SOCLE_SPI_IER, soclespi) |
					SOCLE_SPI_IER_RXFIFO_INT_EN |
					SOCLE_SPI_IER_RX_COMPLETE_INT_EN, soclespi);
		}
	}		

	/* Start SPI transfer */
//	printk("-----------spi_tx_cnt: %d spi_rx_cnt: %d---------", socle_spi_read(0x14, soclespi), socle_spi_read(0x18, soclespi));
/*	socle_spi_write(SOCLE_SPI_FWCR,
		socle_spi_read(SOCLE_SPI_FWCR, soclespi) | SOCLE_SPI_TXRX_RUN, 
		soclespi);*/

	/* Wait for Complete */
//printk("+++++++++++++++++++++++wait end================================1\n");
	wait_event_interruptible(soclespi->wq, soclespi->xfer_stat == xfer_complete);
/*while (socle_spi_read(SOCLE_SPI_FWCR,soclespi) & SOCLE_SPI_TXRX_RUN) 
{
	printk("\nFWCR:%x TX_C:%x RX_C:%x\n", socle_spi_read(SOCLE_SPI_FWCR, soclespi), socle_spi_read(0x14, soclespi), socle_spi_read(0x18, soclespi));
	mdelay(10);
}/**/
	if (soclespi->tx_buf && soclespi->rx_buf)
	{
		t->len= soclespi->rx_xfer_cnt; //2009-04-08: Peter+ for returning correct length
		ret   = soclespi->rx_xfer_cnt;
	}
	else if (soclespi->tx_buf)
	{
		t->len= soclespi->tx_xfer_cnt; //2009-04-08: Peter+ for returning correct length
		ret   = soclespi->tx_xfer_cnt;
	}
	else
	{
		t->len= soclespi->rx_xfer_cnt; //2009-04-08: Peter+ for returning correct length
		ret   = soclespi->rx_xfer_cnt;
	}
	
	soclespi->state &= ~SPIBUSY;
	
	return ret;
}


static int socle_spi_dma_init(struct socle_spi_host *soclespi)
{
	int err;
	dma_cap_mask_t mask;

	/* Try to grab two DMA channels */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	
	/* current we need use filter to find Specified channel ID */
	soclespi->dma_tx_chan = dma_request_channel(mask, NULL, NULL);
	soclespi->dma_rx_chan = dma_request_channel(mask, NULL, NULL);
	if (!soclespi->dma_rx_chan || !soclespi->dma_tx_chan) {
		dev_err(soclespi->dev,
			"DMA channel not available, SPI unable to use DMA\n");
		err = -EBUSY;
		goto error;
	}


	dev_info(soclespi->dev,
			"Using %s (tx) and %s (rx) for DMA transfers\n",
			dma_chan_name(soclespi->dma_tx_chan),
			dma_chan_name(soclespi->dma_rx_chan));
	return 0;
error:
	if (soclespi->dma_rx_chan)
		dma_release_channel(soclespi->dma_rx_chan);
	if (soclespi->dma_tx_chan)
		dma_release_channel(soclespi->dma_tx_chan);
	return err;
}


static irqreturn_t socle_spi_isr(int irq, void *_host)
{
	u32 tmp;
	struct socle_spi_host *soclespi = (struct socle_spi_host *)_host;

	/* Read & clear interrupt status */
	tmp = socle_spi_read(SOCLE_SPI_ISR, soclespi);

	/* Check if the receive data is overrun */
	if (SOCLE_SPI_RXFIFO_OVR_INT == (tmp & SOCLE_SPI_RXFIFO_OVR_INT)) {
		dev_dbg(soclespi->dev, "RX FIFO overrun\n");
		dev_err(soclespi->dev, "receive FIFO is full and another character has been received in the receiver shift register\n");
		
		if (soclespi->tx_buf)
			soclespi->xfer_stat |= TX_XFER_DONE;
		if (soclespi->rx_buf)
			soclespi->xfer_stat |= RX_XFER_DONE;

		wake_up_interruptible(&soclespi->wq);
		goto out;
	}

	/* Check is receive complete */
	if (SOCLE_SPI_RX_COMPLETE_INT == (tmp &SOCLE_SPI_RX_COMPLETE_INT)) {
		dev_dbg(soclespi->dev, "RX completed\n");
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			soclespi->get_rx(soclespi);
		}
		
		/* Disable the rx interrupt */
		socle_spi_write(SOCLE_SPI_IER,
				socle_spi_read(SOCLE_SPI_IER, soclespi)
				&~(SOCLE_SPI_IER_RXFIFO_INT_EN | SOCLE_SPI_IER_RX_COMPLETE_INT_EN)
				, soclespi);
		
		soclespi->xfer_stat |= RX_XFER_DONE;
		wake_up_interruptible(&soclespi->wq);
		goto out;
	}

	/* Check if any RX is available */
	if (SOCLE_SPI_RXFIFO_INT == (tmp & SOCLE_SPI_RXFIFO_INT)) {
		while (SOCLE_SPI_RXFIFO_DATA_AVAIL == (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_RXFIFO_DATA_AVAIL)) {
			soclespi->get_rx(soclespi);
		}
	}

	/* Check if the TX is available */
	if (SOCLE_SPI_TXFIFO_INT == (tmp & SOCLE_SPI_TXFIFO_INT)) {
		while (SOCLE_SPI_TXFIFO_FULL != (socle_spi_read(SOCLE_SPI_FCR, soclespi) & SOCLE_SPI_TXFIFO_FULL)) {
			soclespi->set_tx(soclespi);
			if (soclespi->tx_xfer_cnt == soclespi->len) {/* tx transfer done */				
				socle_spi_write(SOCLE_SPI_IER,
					socle_spi_read(SOCLE_SPI_IER, soclespi) & ~SOCLE_SPI_IER_TXFIFO_INT_EN, 
					soclespi);
				soclespi->xfer_stat |= TX_XFER_DONE;
				wake_up_interruptible(&soclespi->wq);
				break;
			}
		}
	}
out:
	return IRQ_HANDLED;
}

static struct socle_spi_platform_data socle_pdata = {
	.master_enable = 1,
	.wifi_module_use = 0,
};

static const struct of_device_id socle_spi_of_match[] = {
	{
		.compatible = "socle,general-spi",
		.data = &socle_pdata,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, socle_spi_of_match);

static int socle_spi_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct socle_spi_host	*soclespi;
	struct resource		*r;
	int			status = 0;
	static int		bus_num = 2;
	const struct of_device_id *match;
	unsigned int num_cs;
	struct device_node *node = pdev->dev.of_node;

		
	dev_dbg(&pdev->dev, "\nsocle_spi_probe()\n");	
	
	/* Get Platform data */
	match = of_match_device(socle_spi_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	socle_spi_info = match->data;	
	/*
	 * default num_cs is 1 and all chipsel are internal to the chip
	 * indicated by chip_sel being NULL. 
	 */
	num_cs = 1;
	of_property_read_u32(node, "num-cs", &num_cs);
	
	
	/* Create SPI Master */
	master = spi_alloc_master(&pdev->dev, sizeof *soclespi);
	if (master == NULL) {
		dev_dbg(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}
		
	soclespi = spi_master_get_devdata(master);	
	memset(soclespi, 0, sizeof(struct socle_spi_host));	
	soclespi->master = spi_master_get(master);
	soclespi->master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST | SPI_LOOP;
	soclespi->master->bus_num = bus_num++;
//	soclespi->master->bus_num = -1;
	soclespi->master->num_chipselect = num_cs;
	soclespi->dev = &pdev->dev;	
	platform_set_drvdata(pdev, soclespi);
//	platform_set_drvdata(pdev, master);

	soclespi->master->dev.of_node = node;//kobecheng
		
	init_waitqueue_head(&soclespi->wq);

	/* initial the spi master setup flag */
	soclespi->master_setup = 0;

	/* Setup the state for bitbang driver */
	soclespi->bitbang.chipselect = socle_spi_chipselect;
	soclespi->bitbang.setup_transfer = socle_spi_setupxfer;
	soclespi->bitbang.txrx_bufs = socle_spi_txrx_bufs;
	soclespi->bitbang.master = soclespi->master;
	soclespi->bitbang.master->setup = socle_spi_setup;

	/*
	 * Scratch buffer is used for throwaway rx and tx data.
	 * It's coherent to minimize dcache pollution.
	 */
//	soclespi->buffer = dma_alloc_coherent(&pdev->dev, BUFFER_SIZE,
//					&soclespi->buffer_dma, GFP_KERNEL);
//	if (!soclespi->buffer)
//		goto out_free;
		
	if(socle_spi_dma_init(soclespi))
		goto free_master;
		
	of_property_read_u32(node, "dma-tx-id", &soclespi->dma_tx_slave_id);
	of_property_read_u32(node, "dma-rx-id", &soclespi->dma_rx_slave_id);
	printk("dma-tx-id= %d\n",soclespi->dma_tx_slave_id);
	printk("dma-rx-id= %d\n",soclespi->dma_rx_slave_id);
//	printk("dma-tx-chan_id= %d\n",soclespi->dma_tx_chan->chan_id);
	//kobecheng
	soclespi->dma_tx_chan->chan_id = soclespi->dma_tx_slave_id;
	soclespi->dma_rx_chan->chan_id = soclespi->dma_rx_slave_id;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		status = -ENODEV;
		goto free_master;
	}
	soclespi->spi_fifo_addr = r->start;	
	/* SPI Base */
	soclespi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(soclespi->base)) {
		status = PTR_ERR(soclespi->base);
		goto free_master;
	}
	
	/*Master mode setting*/
	if (socle_spi_info->master_enable)
		socle_spi_write(SOCLE_SPI_FWCR,
			socle_spi_read(SOCLE_SPI_FWCR, soclespi) |SOCLE_SPI_MODE_MASTER, 
			soclespi);

	/* Reset SPI controller */
	socle_spi_write(SOCLE_SPI_FWCR,
  		socle_spi_read(SOCLE_SPI_FWCR, soclespi) |SOCLE_SPI_MASTER_SOFT_RST, 
  		soclespi);
	
	soclespi->irq = platform_get_irq(pdev, 0);
	if (soclespi->irq < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		goto free_master;
	}
	snprintf(soclespi->spi_name, sizeof(soclespi->spi_name), "%s %d", pdev->name, soclespi->master->bus_num);
	/* Allocate the interrupt */
	status = request_irq(soclespi->irq, socle_spi_isr, IRQF_DISABLED, soclespi->spi_name, soclespi);
	if (status) {
		dev_err(&pdev->dev, "cannot claim IRQ\n");
		goto free_master;
	}
	/* Get SPI clk */
	soclespi->clk = devm_clk_get(&pdev->dev, "spi_mclk");
	if (IS_ERR(soclespi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		goto err_register;
	}
	clk_prepare_enable(soclespi->clk);
	soclespi->baseclk = clk_get_rate(soclespi->clk);
	printk("SPI CLK=0x%x\n",soclespi->baseclk);	
		
	
	/* Register our spi controller */
	status = spi_bitbang_start(&soclespi->bitbang);
	if (status) {
		dev_err(&pdev->dev, "failed to register SPI master\n");
		goto err_register;
	}

	return 0;
	
err_register:	
	free_irq(soclespi->irq, soclespi);
free_master:
	spi_master_put(master);
	return status;
}


static int socle_spi_remove(struct platform_device *pdev)
{
	struct socle_spi_host *soclespi = platform_get_drvdata(pdev);
	
	dev_dbg(soclespi->dev, "socle_spi_remove()\n");
	
	spi_bitbang_stop(&soclespi->bitbang);
	platform_set_drvdata(pdev, NULL);
	free_irq(soclespi->irq, soclespi);
	spi_master_put(soclespi->master);

	return 0;
}

#ifdef CONFIG_PM
static int socle_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct socle_spi_host *soclespi = platform_get_drvdata(pdev);
	
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
	struct socle_spi_host *soclespi = platform_get_drvdata(pdev);
	struct spi_device *spi = soclespi->spi_dev;
	int err;
		
	/* Reset SPI controller */
	socle_spi_write(SOCLE_SPI_FWCR,
  		socle_spi_read(SOCLE_SPI_FWCR, soclespi) |SOCLE_SPI_MASTER_SOFT_RST, 
  		soclespi);

	/* if this spi master haved setuped, setup again at resume */ 
	if (soclespi->master_setup) {
		err = socle_spi_setupxfer(spi, NULL);
		if (err)
			dev_err(&spi->dev, "setup_transfer returned %d\n", err);
	}
	
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
		.name =		"Socle-SPI",
		.owner =	THIS_MODULE,
		.of_match_table = socle_spi_of_match,
	},
	.probe =	socle_spi_probe,
	.remove =	socle_spi_remove,
	.suspend = 	socle_spi_suspend,
	.resume = 	socle_spi_resume,
};

static int __init socle_spi_init(void)
{
	int ret;

	ret = platform_driver_register(&SOCLE_SPI_driver);
	pr_info("Socle SPI subsystem %s\n", ret ? "failed" : "initialized");

	return ret;
}

/*arch_initcall(socle_spi_init);kobecheng*/
module_init(socle_spi_init);/**/

/*module_platform_driver(SOCLE_SPI_driver);*/
MODULE_DESCRIPTION("Socle SPI Master Driver");
MODULE_LICENSE("GPL");
