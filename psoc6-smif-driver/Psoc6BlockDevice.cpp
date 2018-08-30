#include "Psoc6BlockDevice.h"
#include "cycfg_smif_memslot.h"

#include "cy_gpio.h"
#include "cy_sysint.h"
#include "cy_sysclk.h"

#define SMIF_PRIORITY       		(1u)      		/* SMIF interrupt priority */
#define SMIF_DESELECT_DELAY 		(7u)            /**< Minimum duration of SPI de-selection */
#define SMIF_DEBUG					(0)
#define TIMEOUT_1_MS        		(5000ul)  		/* 1 ms timeout for all blocking functions */
#define ADDRESS_SIZE        		(3u)      		/* Memory address size */

#define PSOC6_PROGRAM_BUFFER_SIZE	(512UL)			/* S25-FL512S */
#define PSOC6_ERASE_SECTOR_SIZE		(256*1024UL)	/* S25-FL512S - 256KBytes */
#define PSOC6_DEVICE_SIZE			(0x4000000UL)	/* S25-FL512S - 64MBytes */

cy_stc_smif_context_t SMIF_context;

cy_stc_smif_config_t const Psoc6BlockDevice::SMIF_config =
{
    .mode           = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay  = SMIF_DESELECT_DELAY,
    .rxClockSel     = (uint32_t)CY_SMIF_SEL_INV_INTERNAL_CLK,
    .blockEvent     = (uint32_t)CY_SMIF_BUS_ERROR
};

static void SMIF_Interrupt_User(void)
{
    Cy_SMIF_Interrupt(SMIF0, &SMIF_context);
}


#include <stdio.h>

#if(SMIF_DEBUG)
void CheckStatus(const char * msg, uint32_t status)
{
    if(0 != status) {
        printf("%s", msg);
        __disable_irq();
        while(1u);		/* Error handle*/
    }
}

void PrintArray(const char * msg, uint8_t * buff, uint32_t size)
{
    printf("%s", msg);
    for(uint32_t index=0; index<size; index++) {
        printf("0x%02X ", (unsigned int) buff[index]);
    }
    printf("\r\n=======================\r\n");
}
#endif

Psoc6BlockDevice::Psoc6BlockDevice()
: _is_initialized(false), _block_size(PSOC6_PROGRAM_BUFFER_SIZE), _erase_size(PSOC6_ERASE_SECTOR_SIZE), _device_size(PSOC6_DEVICE_SIZE)
{
}

Psoc6BlockDevice::~Psoc6BlockDevice()
{
}

int Psoc6BlockDevice::init()
{
	if (!_is_initialized) {
        /* Configure SMIF interrupt */
		cy_stc_sysint_t smifIntConfig = {
			.intrSrc = smif_interrupt_IRQn,     /* SMIF interrupt */
			.intrPriority = SMIF_PRIORITY       /* SMIF interrupt priority */
		};

		/* SMIF interrupt initialization status */
		cy_en_sysint_status_t intr_init_status;
		intr_init_status = Cy_SysInt_Init(&smifIntConfig, SMIF_Interrupt_User);
		if(intr_init_status != CY_SYSINT_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Initialize SMIF */
		cy_en_smif_status_t smif_status;
		smif_status = Cy_SMIF_Init(SMIF0, &SMIF_config, TIMEOUT_1_MS, &SMIF_context);
		if(smif_status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Configures data select */
		Cy_SMIF_SetDataSelect(SMIF0, CY_SMIF_SLAVE_SELECT_0, CY_SMIF_DATA_SEL0);
		Cy_SMIF_Enable(SMIF0, &SMIF_context);

		/* Enable the SMIF interrupt */
		NVIC_EnableIRQ(smif_interrupt_IRQn);

		_is_initialized = true;
	}

    return 0;
}

int Psoc6BlockDevice::deinit()
{
    if (_is_initialized) {
    	Cy_SMIF_DeInit(SMIF0);
		_is_initialized = false;
    }
    return 0;
}

int Psoc6BlockDevice::sync()
{
    return 0;
}

int Psoc6BlockDevice::read(void *buffer, bd_addr_t addr, bd_size_t size)
{
	if (!_is_initialized) {
		return BD_ERROR_DEVICE_ERROR;
	}

	// Check the address and size fit onto the chip.
//    MBED_ASSERT(is_valid_read(addr, size));

	cy_en_smif_status_t status;
	uint8_t rxBuffer_reg;
	uint8_t address[ADDRESS_SIZE];
	cy_stc_smif_mem_device_cfg_t *device = smifMemConfigs[0]->deviceCfg;
	cy_stc_smif_mem_cmd_t *cmdreadStsRegQe = device->readStsRegQeCmd;

	address[0u] = (addr>>16) & 0xff;;
	address[1u] = (addr>>8) & 0xff;
	address[2u] = addr & 0xff;

	/* Set QE */
	status = Cy_SMIF_Memslot_QuadEnable(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context);
#if(SMIF_DEBUG)
	CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_QuadEnable failed\r\n", status);
#endif
	if(status != CY_SMIF_SUCCESS) {
		return BD_ERROR_DEVICE_ERROR;
	}
	/* Wait till memory is ready after Quad Enable operation */
	while(Cy_SMIF_Memslot_IsBusy(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context)){

	};

	/* Read data from the external memory configuration register */
	status = Cy_SMIF_Memslot_CmdReadSts(SMIF0, smifMemConfigs[0], &rxBuffer_reg, (uint8_t)cmdreadStsRegQe->command , &SMIF_context);
#if(SMIF_DEBUG)
	CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdReadSts failed\r\n", status);
	printf("Received Data: 0x%X\r\n", (unsigned int) rxBuffer_reg);
	printf("\r\nQuad I/O Read (QIOR 0x%0X) \r\n", 0x38);
#endif
	if(status != CY_SMIF_SUCCESS) {
		return BD_ERROR_DEVICE_ERROR;
	}

	/* The 4 Page program command */
	status = Cy_SMIF_Memslot_CmdRead(SMIF0, smifMemConfigs[0], address, (uint8_t *)buffer, size, NULL, &SMIF_context);
#if(SMIF_DEBUG)
	CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdRead failed\r\n",status);
#endif
	if(status != CY_SMIF_SUCCESS) {
		return BD_ERROR_DEVICE_ERROR;
	}

	/* Wait until the SMIF IP operation is completed. */
	while(Cy_SMIF_BusyCheck(SMIF0));

	/* Send received data to the console */
#if(SMIF_DEBUG)
	Cy_SysLib_Delay(100);
	PrintArray("Received Data: ", (uint8_t *)buffer, size);
#endif

	return 0;
}

int Psoc6BlockDevice::program(const void *buffer, bd_addr_t addr, bd_size_t size)
{
	if (!_is_initialized) {
		return BD_ERROR_DEVICE_ERROR;
	}

	cy_en_smif_status_t status;
	uint8_t address[ADDRESS_SIZE];
	uint8_t rxBuffer_reg;
	cy_stc_smif_mem_device_cfg_t *device = smifMemConfigs[0]->deviceCfg;
	cy_stc_smif_mem_cmd_t *cmdreadStsRegQe = device->readStsRegQeCmd;
	cy_stc_smif_mem_cmd_t *cmdreadStsRegWip = device->readStsRegWipCmd;

	// Check the address and size fit onto the chip.
//    MBED_ASSERT(is_valid_program(addr, size));

	while (size > 0) {
		// Write up to PSOC6_PROGRAM_BUFFER_SIZE bytes a page
		uint32_t off = addr % PSOC6_PROGRAM_BUFFER_SIZE;
		uint32_t chunk = ((off + size) < PSOC6_PROGRAM_BUFFER_SIZE) ? size : (PSOC6_PROGRAM_BUFFER_SIZE - off);

		/* Set QE */
		status = Cy_SMIF_Memslot_QuadEnable(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_QuadEnable failed\r\n", status);
#endif
		if(status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Wait till memory is ready after Quad Enable operation */
		while(Cy_SMIF_Memslot_IsBusy(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context));

		/* Read data from the external memory configuration register */
		status = Cy_SMIF_Memslot_CmdReadSts(SMIF0, smifMemConfigs[0], &rxBuffer_reg, (uint8_t)cmdreadStsRegQe->command , &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdReadSts failed\r\n", status);
		printf("Received Data: 0x%X\r\n", (unsigned int)rxBuffer_reg);
#endif
		if(status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Send Write Enable to external memory */
		status = Cy_SMIF_Memslot_CmdWriteEnable(SMIF0, smifMemConfigs[0], &SMIF_context);
#if(SMIF_DEBUG)
	    CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdWriteEnable failed\r\n", status);
		printf("\r\nQuad Page Program (QPP 0x%0X) \r\n", 0x38);
#endif
		if(status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Quad Page Program command */
		address[0u] = (addr>>16) & 0xff;
		address[1u] = (addr>>8) & 0xff;
		address[2u] = addr & 0xff;

		status = Cy_SMIF_Memslot_CmdProgram(SMIF0, smifMemConfigs[0], address, (uint8_t *)buffer, chunk, NULL, &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdProgram failed\r\n", status);
        PrintArray("Written Data: ", (uint8_t *)buffer, chunk);
#endif
		if(status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}
		/* Wait until the Write operation is completed */
		while(Cy_SMIF_Memslot_IsBusy(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context));

		/* Read data from the external memory status register */
		status = Cy_SMIF_Memslot_CmdReadSts(SMIF0, smifMemConfigs[0], &rxBuffer_reg,
								 (uint8_t)cmdreadStsRegWip->command , &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF ReadStatusReg failed\r\n", status);
		printf("Received Data: 0x%X\r\n", (unsigned int)rxBuffer_reg);
#endif
		if(status != CY_SMIF_SUCCESS) {
			return BD_ERROR_DEVICE_ERROR;
		}

		if(size > chunk) {
			buffer = (const uint8_t*)(buffer) + chunk;
			addr += chunk;
			size -= chunk;
		}
		else {
			size = 0;
		}
	}

	return 0;
}

int Psoc6BlockDevice::erase(bd_addr_t addr, bd_size_t size)
{
	if (!_is_initialized) {
		return BD_ERROR_DEVICE_ERROR;
	}

	cy_en_smif_status_t status;
	uint8_t address[ADDRESS_SIZE];

	// Check the address and size fit onto the chip.
//    MBED_ASSERT(is_valid_erase(addr, size));

	while (size > 0) {
		uint32_t chunk = PSOC6_ERASE_SECTOR_SIZE;

		status = Cy_SMIF_Memslot_CmdWriteEnable(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdWriteEnable failed\r\n", status);
#endif
		if(status != CY_SMIF_SUCCESS){
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Quad Page Program command */
		address[0u] = (addr>>16) & 0xff;;
		address[1u] = (addr>>8) & 0xff;
		address[2u] = addr & 0xff;

		status = Cy_SMIF_Memslot_CmdSectorErase(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], address, &SMIF_context);
#if(SMIF_DEBUG)
		CheckStatus("\r\n\r\nSMIF Cy_SMIF_Memslot_CmdSectorErase failed\r\n", status);
#endif
		if(status != CY_SMIF_SUCCESS){
			return BD_ERROR_DEVICE_ERROR;
		}

		/* Wait until the Erase operation is completed */
		while(Cy_SMIF_Memslot_IsBusy(SMIF0, (cy_stc_smif_mem_config_t*)smifMemConfigs[0], &SMIF_context));

		if(size > chunk) {
			addr += chunk;
			size -= chunk;
		}
		else
		{
			size = 0;
		}
	}

	return 0;
}

bd_size_t Psoc6BlockDevice::get_read_size() const
{
	return _block_size;
}

bd_size_t Psoc6BlockDevice::get_program_size() const
{
	return _block_size;
}

bd_size_t Psoc6BlockDevice::get_erase_size() const
{
	return _erase_size;
}

bd_size_t Psoc6BlockDevice::get_erase_size(bd_addr_t addr) const
{
	return _erase_size;
}

bd_size_t Psoc6BlockDevice::size() const
{
    return _device_size;
}

int Psoc6BlockDevice::get_erase_value() const
{
	return 0xFF;
}
