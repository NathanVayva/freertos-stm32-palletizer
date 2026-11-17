#include "main.h"
#include "factory_io.h"
#include <string.h>

#define SUBSCRIBER_TABLE_LENGTH (5)
#define SENSOR_TABLE_SIZE        (14)

#define NOTIFY_INDEX_PUBLISHER	0
#define NOTIFY_INDEX_ASCENSEUR_PALETTE	1
#define NOTIFY_INDEX_BLOQUEUR_CARTON	1
#define NOTIFY_INDEX_POUSSOIR_PORTE		1

#define SUBSCRIBE_QUEUE_SIZE			5
#define COMMANDE_QUEUE_SIZE				12



// Typedefs
typedef struct
{
	xTaskHandle task_handle ; // task handle to notify
	uint32_t sensor_id; // Awaited sensor ID
	uint32_t sensor_state; // Awaited sensor State
} subscribe_message_t;

typedef struct
{
	uint8_t msg_type;             // 1 => Bridge / 0 => printf
	uint8_t msg[CMD_MSG_SIZE];    // For printf debug
	uint32_t act_msk;            // Which actuator to change
	uint32_t act_cmd;            // State of changed actuator
}command_message_t;


// Static functions
static void SystemClock_Config(void);
static subscribe_message_t subscribe(xTaskHandle handle, uint32_t sensor_id, uint32_t sensor_state);
static command_message_t commande(uint32_t act_msk, uint32_t act_cmd);

// FreeRTOS tasks
void Distribution_carton	(void *pvParameters);
void bloqueur_poussoir		(void *pvParameters);
void porte					(void *pvParameters);
void ascenseur				(void *pvParameters);
void distribution_palette	(void *pvParameters);
void vTask_pub				(void *pvParameters);
void vTask_write			(void *pvParameters);


// Task Handles
xTaskHandle		Distribution_carton_handle;
xTaskHandle 	bloqueur_poussoir_handle;
xTaskHandle 	porte_handle;
xTaskHandle 	ascenseur_handle;
xTaskHandle 	distribution_palette_handle;
xTaskHandle		vTask_pub_handle;
xTaskHandle		vTask_write_handle;
//xTaskHandle vTaskHWM_handle;

// Kernel objects
xQueueHandle     xSubscribeQueue;
xQueueHandle     xCommandQueue;


// Trace User Events Channels
//traceString ue1, ue2, ue3;

// Global variables
uint8_t tx_dma_buffer[CMD_MSG_SIZE];
uint8_t rx_dma_buffer[SS_MSG_SIZE];

// Main function
int main()
{
	uint32_t    free_heap_size;

	// Configure System Clock
	//SystemClock_Config();

	// Initialize LED pin
	BSP_LED_Init();

	// Initialize Debug Console
	BSP_Console_Init();
	//    my_printf("Console Ready!\r\n");

	// Start Trace Recording
	vTraceEnable(TRC_START);

	// Create Kernel object
	xSubscribeQueue = xQueueCreate(SUBSCRIBE_QUEUE_SIZE, sizeof(subscribe_message_t));
	vTraceSetQueueName(xSubscribeQueue, "Subscribe Queue");
	xCommandQueue = xQueueCreate(COMMANDE_QUEUE_SIZE, sizeof(command_message_t));
	vTraceSetQueueName(xCommandQueue, "Command Queue");


	// Create Tasks
	xTaskCreate(Distribution_carton,    "Distribution_carton",    124, NULL, 1, &Distribution_carton_handle);
	xTaskCreate(bloqueur_poussoir,    "bloqueur_poussoir",    126, NULL, 1, &bloqueur_poussoir_handle);
	xTaskCreate(porte,    "porte",    124, NULL, 1, &porte_handle);
	xTaskCreate(ascenseur,    "ascenseur",    134, NULL, 1, &ascenseur_handle);
	xTaskCreate(distribution_palette,    "distribution_palette",    126, NULL, 1, &distribution_palette_handle);


	xTaskCreate(vTask_pub,    "Task_Pub",    140, NULL, 2, &vTask_pub_handle);
	xTaskCreate(vTask_write,    "Task_Write",    117, NULL, 2, &vTask_write_handle);


	// Start the Scheduler
	vTaskStartScheduler();
	//    my_printf("Console Ready and scheduler started!\r\n");
	free_heap_size = xPortGetFreeHeapSize();
	my_printf("\r\nFree Heap Size is %d bytes\r\n", free_heap_size);


	while(1)
	{
		// The program should never be here...
		//        my_printf("Big Oops!");
	}
}


/*
 *    Task_1
 */
void Distribution_carton (void *pvParameters)
{
	subscribe_message_t sub_msg;
	command_message_t cmd_msg;

	cmd_msg.msg_type = 1;
	cmd_msg.act_msk = Tapis_Distribution_Cartons | Tapis_Carton_vers_Palettiseur | Tapis_Distribution_Palette | Tapis_Palette_Vers_Ascenseur | Tapis_Fin | Charger_Palettiseur | Remover;
	cmd_msg.act_cmd = Tapis_Distribution_Cartons | Tapis_Carton_vers_Palettiseur | Tapis_Distribution_Palette | Tapis_Palette_Vers_Ascenseur | Tapis_Fin | Charger_Palettiseur | Remover;
	xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

	while(1)
	{
		cmd_msg = commande(Distribution_Cartons, Distribution_Cartons);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		// Wait for entree palettiseur == 0
		sub_msg = subscribe(Distribution_carton_handle, Carton_Envoye, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		// ==1
		sub_msg = subscribe(Distribution_carton_handle, Carton_Envoye, Carton_Envoye);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		// ==0
		sub_msg = subscribe(Distribution_carton_handle, Carton_Envoye, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		//        cmd_msg.msg_type = 1;
		//        cmd_msg.act_msk = Distribution_Cartons;
		//        cmd_msg.act_cmd = 0;
		cmd_msg = commande(Distribution_Cartons, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);


		//xSemaphoreTake(xSemDistrib_Carton, portMAX_DELAY);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_BLOQUEUR_CARTON,pdTRUE, portMAX_DELAY);


	}
}
/*
 *    Task_2
 */
void bloqueur_poussoir (void *pvParameters)
{
	//    BSP_PB_Init();
	//
	//    // Set priority for EXTI line 4 to 15, and enable interrupt
	//    NVIC_SetPriority(EXTI4_15_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY + 0);
	//    NVIC_EnableIRQ(EXTI4_15_IRQn);

	subscribe_message_t sub_msg;
	command_message_t cmd_msg;

	int cmpt_carton = 0;

	while(1)
	{
		cmd_msg = commande(Blocage_Entree_Palettiseur, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		sub_msg = subscribe(bloqueur_poussoir_handle, Entree_Palettiseur, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(bloqueur_poussoir_handle, Entree_Palettiseur, Entree_Palettiseur);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(bloqueur_poussoir_handle, Entree_Palettiseur, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(bloqueur_poussoir_handle, Entree_Palettiseur, Entree_Palettiseur);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		vTaskDelay(2000);

		//bloquage
		cmd_msg = commande(Blocage_Entree_Palettiseur, Blocage_Entree_Palettiseur);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		//envoi carton
		xTaskNotifyGiveIndexed(Distribution_carton_handle, NOTIFY_INDEX_BLOQUEUR_CARTON);

		vTaskDelay(1000);

		//poussoir aller
		cmd_msg = commande(Poussoir, Poussoir);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(400);

		sub_msg = subscribe(bloqueur_poussoir_handle, Limite_Poussoir, Limite_Poussoir);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		//poussoir retour
		cmd_msg = commande(Poussoir, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(400);

		sub_msg = subscribe(bloqueur_poussoir_handle, Limite_Poussoir, Limite_Poussoir);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		cmpt_carton+=1;
		if (cmpt_carton == 3)
		{
			cmpt_carton = 0;
			xTaskNotifyGiveIndexed(porte_handle, NOTIFY_INDEX_POUSSOIR_PORTE);
		}
	}
}

void porte(void *pvParameters)
{
	subscribe_message_t sub_msg;
	command_message_t cmd_msg;

	while(1)
	{

		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_POUSSOIR_PORTE, pdTRUE, portMAX_DELAY);

		// Clamp
		cmd_msg = commande(Clamp, Clamp);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		// arret tapis
		cmd_msg = commande(Tapis_Carton_vers_Palettiseur, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		// Synchro ascenseur 1
		sub_msg = subscribe(porte_handle,Ascenseur_Etage_2 | Ascenseur_en_mouvement, Ascenseur_Etage_2 );
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);


		//ouverture porte
		cmd_msg = commande(Porte, Porte);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(300);
		sub_msg = subscribe(porte_handle, Limite_Porte, Limite_Porte);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		vTaskDelay(1000);

		//declamp
		sub_msg = subscribe(porte_handle, Clamped, Clamped);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		cmd_msg = commande(Clamp, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		//ascenseur descent
		sub_msg = subscribe(porte_handle, Ascenseur_en_mouvement, Ascenseur_en_mouvement);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(porte_handle, Ascenseur_en_mouvement, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

//		sub_msg = subscribe(porte_handle,Ascenseur_Etage_1 , 0);
//		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
//		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		//fermeture porte
		cmd_msg = commande(Porte, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(300);
		sub_msg = subscribe(porte_handle, Limite_Porte, Limite_Porte);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		//marche tapis
		cmd_msg = commande(Tapis_Carton_vers_Palettiseur, Tapis_Carton_vers_Palettiseur);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
	}
}

void ascenseur(void *pvParameters)
{
	subscribe_message_t sub_msg;
	command_message_t cmd_msg;

	cmd_msg = commande(Charger_Palette, Charger_Palette);
	xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

	while(1)
	{


		//sub_msg = subscribe(ascenseur_handle, Sortie_Palette, Sortie_Palette);
		//xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(ascenseur_handle, Entree_Palette, Entree_Palette);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);
		vTaskDelay(2270); //magic number

		cmd_msg = commande(Charger_Palette | Ascenseur_To_Limit | Monter_Ascenseur | Descendre_Ascenseur, Ascenseur_To_Limit | Monter_Ascenseur);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		sub_msg = subscribe(ascenseur_handle, Porte_Ouverte, Porte_Ouverte);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		cmd_msg = commande( Ascenseur_To_Limit | Monter_Ascenseur, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(1000);

		cmd_msg = commande(Descendre_Ascenseur , Descendre_Ascenseur);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		sub_msg = subscribe(ascenseur_handle, Porte_Ouverte, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		sub_msg = subscribe(ascenseur_handle, Porte_Ouverte, Porte_Ouverte);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		vTaskDelay(1000);

		cmd_msg = commande(Descendre_Ascenseur | Ascenseur_To_Limit , Descendre_Ascenseur| Ascenseur_To_Limit);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		sub_msg = subscribe(ascenseur_handle, Ascenseur_en_mouvement, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);

		vTaskDelay(200);

		xTaskNotifyGiveIndexed(distribution_palette_handle, NOTIFY_INDEX_ASCENSEUR_PALETTE);

		cmd_msg = commande(Charger_Palette , Charger_Palette );
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		sub_msg = subscribe(ascenseur_handle, Sortie_Palette, 0);
		xQueueSendToBack(xSubscribeQueue, &sub_msg, 0);
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_PUBLISHER,pdTRUE, portMAX_DELAY);


	}
}

void distribution_palette(void *pvParameters)
{
	//	subscribe_message_t sub_msg;
	command_message_t cmd_msg;

	while(1)
	{
		// Distribue palette
		cmd_msg = commande(Distribution_Palette, Distribution_Palette);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		vTaskDelay(4000); // Delai distribution palette 3s a 4s

		cmd_msg = commande(Distribution_Palette, 0);
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		// Synchro ascensseur
		ulTaskNotifyTakeIndexed(NOTIFY_INDEX_ASCENSEUR_PALETTE,pdTRUE, portMAX_DELAY);
	}
}









/*
 *    Task_Pub
 */
void vTask_pub (void *pvParameters)
{
	portBASE_TYPE   xStatus;

	subscribe_message_t msg;

	command_message_t cmd_msg;

	subscribe_message_t sub_tab[SUBSCRIBER_TABLE_LENGTH];
	int8_t first_free_index;
	uint8_t already_there;

	uint32_t sensor = 0;

	memset(sub_tab, 0, sizeof(subscribe_message_t) * SUBSCRIBER_TABLE_LENGTH);
	portTickType    xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

	while(1)
	{
		//        vTaskDelay(1000);

		// Update
		cmd_msg.msg[0] = TAG_UPDATE;
		cmd_msg.msg[1] = '\n';
		cmd_msg.msg_type = 0;
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);

		xStatus = xQueueReceive(xSubscribeQueue, &msg, 0);

		if (xStatus == pdPASS)
		{
			my_sprintf((char*)(cmd_msg.msg),"\r\nSubscribing : Handle=%d SensID=%d State=%d\r\n", msg.task_handle, msg.sensor_id, msg.sensor_state);
			cmd_msg.msg_type = 0;
			xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
			first_free_index = -1;
			already_there = 0;

			for (int i = 0; i < SUBSCRIBER_TABLE_LENGTH; ++i) {
				// Get first free cell index
				if (sub_tab[i].task_handle == 0)
				{
					if(first_free_index == -1)
						first_free_index = i;
				}
				// Check if already in table
				else if ((msg.task_handle == sub_tab[i].task_handle) && (msg.sensor_id == sub_tab[i].sensor_id) && (msg.sensor_state == sub_tab[i].sensor_state))
				{
					already_there = 1;
				}
			}
			// Add subscription if necessary
			if (already_there == 0)
			{
				my_sprintf((char*)(cmd_msg.msg),"Adding subscription in slot [%d]\r\n", first_free_index);
				cmd_msg.msg_type = 0;
				xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
				if (first_free_index != -1)
					sub_tab[first_free_index] = msg;
				else
				{
					my_sprintf((char*)(cmd_msg.msg),"ERROR : Table full, cannot add subscription");
					cmd_msg.msg_type = 0;
					xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
				}
			}
			else
			{
				my_sprintf((char*)(cmd_msg.msg),"Subscription already exists\r\n");
				cmd_msg.msg_type = 0;
				xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
			}

			// Table display
			//            for (int i = 0; i < SUBSCRIBER_TABLE_LENGTH; ++i) {
			//                my_sprintf((char*)(cmd_msg.msg),"[%d] %d %d %d\r\n", i, sub_tab[i].sem_id, sub_tab[i].sensor_id, sub_tab[i].sensor_state);
			//                cmd_msg.msg_type = 0;
			//                xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
			//            }

		}
		else
		{
			//            my_sprintf((char*)(cmd_msg.msg),".");
			//            cmd_msg.msg_type = 0;
			//            xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
		}


		// Build 32-bit sensors states representation
		sensor = 0x00000000;

		sensor |= rx_dma_buffer[1];
		sensor |= (rx_dma_buffer[2] <<8U );
		sensor |= (rx_dma_buffer[3] <<16U);
		sensor |= (rx_dma_buffer[4] <<24U);

		my_sprintf((char*)(cmd_msg.msg),"\r\nSensors state = %u\r\n", sensor);
		cmd_msg.msg_type = 0;
		xQueueSendToBack(xCommandQueue, &cmd_msg, 0);


		// Check sensors
		for (int i = 0; i < SUBSCRIBER_TABLE_LENGTH; ++i) {
			if(sub_tab[i].task_handle !=0)
			{
				// if (sub_tab[i].sensor_state == (sensor & (1 << sub_tab[i].sensor_id)) >> sub_tab[i].sensor_id)
				if (sub_tab[i].sensor_state == (sensor & sub_tab[i].sensor_id))
				{
					// Notify
					xTaskNotifyGiveIndexed(sub_tab[i].task_handle, NOTIFY_INDEX_PUBLISHER);

					my_sprintf((char*)(cmd_msg.msg),"\r\nPublishing : Handle=%d SensID=%d State=%d\r\n", sub_tab[i].task_handle, sub_tab[i].sensor_id, sub_tab[i].sensor_state);
					cmd_msg.msg_type = 0;
					xQueueSendToBack(xCommandQueue, &cmd_msg, 0);
					sub_tab[i].task_handle = 0; sub_tab[i].sensor_id = 0; sub_tab[i].sensor_state = 0;

				}

			}
		}

		vTaskDelayUntil (&xLastWakeTime, (200/portTICK_RATE_MS));
	}
}

void vTask_write (void *pvParameters)
{
	command_message_t msg;
	uint32_t actuator_state = 0;
	uint8_t i;

	// Set maximum priority for EXTI line 4 to 15 interrupts
	//NVIC_SetPriority(USART2_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY + 1);

	// Enable EXTI line 4 to 15 (user button on line 13) interrupts
	//NVIC_EnableIRQ(USART2_IRQn);

	// Set maximum priority for EXTI line 4 to 15 interrupts
	NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn, configMAX_API_CALL_INTERRUPT_PRIORITY);

	// Enable EXTI line 4 to 15 (user button on line 13) interrupts
	NVIC_EnableIRQ(DMA1_Channel4_5_6_7_IRQn);


	while(1)
	{
		// Wait for something in the message Queue
		xQueueReceive(xCommandQueue, &msg, portMAX_DELAY);

		if (msg.msg_type == 1)
		{
			actuator_state &= ~msg.act_msk;
			actuator_state |= msg.act_cmd;

			// Prepare frame buffer
			tx_dma_buffer[0] = TAG_ACTUATORS;                                     // Actuators tag

			tx_dma_buffer[1] = (uint8_t) (actuator_state & 0x000000FF);            // data byte #1
			tx_dma_buffer[2] = (uint8_t)((actuator_state & 0x0000FF00) >>8U );    // data byte #2
			tx_dma_buffer[3] = (uint8_t)((actuator_state & 0x00FF0000) >>16U);    // data byte #2
			tx_dma_buffer[4] = (uint8_t)((actuator_state & 0xFF000000) >>24U);    // data byte #2

			tx_dma_buffer[5] = 0x00;                                            // CRC (not yet implemented)
			tx_dma_buffer[6] = '\n';                                            // End byte

			i = 7;
		}
		else
		{
			i=0;
			while(msg.msg[i]!= '\0'){
				tx_dma_buffer[i]=msg.msg[i];
				i++;
			}
			tx_dma_buffer[i]='\0';
		}


		// Set Memory Buffer
		DMA1_Channel4->CNDTR = i;
		// Enable DMA1
		DMA1_Channel4->CCR |= DMA_CCR_EN;
		//DMA Enable Transmitter
		USART2->CR3 |= USART_CR3_DMAT;

		//xSemaphoreTake(xSem_DMA_TX, portMAX_DELAY);
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		//Désactivez le canal DMA
		DMA1_Channel4->CCR &= ~DMA_CCR_EN;
		//Désactivez l’appel au DMA de l’USART2 TX
		USART2->CR3 &= ~USART_CR3_DMAT;
	}
}
/*
 * vTaskHWM
 */
//void vTaskHWM (void *pvParameters)
//{
//    uint32_t    free_heap_size;
//
//    while(1)
//    {
//        // Periodically Report Free Heap size after scheduler has started
//        free_heap_size = xPortGetFreeHeapSize();
//        my_printf("Free Heap Size is %d bytes\r", free_heap_size);
//
//        // Wait for 500ms
//        vTaskDelay(500);
//    }
//}

subscribe_message_t subscribe(xTaskHandle handle, uint32_t sensor_id, uint32_t sensor_state)
{
	subscribe_message_t sub_msg;
	sub_msg.task_handle = handle;
	sub_msg.sensor_id = sensor_id;
	sub_msg.sensor_state = sensor_state;
	return sub_msg;
}

command_message_t commande(uint32_t act_msk, uint32_t act_cmd)
{
	command_message_t cmd_msg;
	cmd_msg.msg_type = 1;
	cmd_msg.act_msk = act_msk;
	cmd_msg.act_cmd = act_cmd;
	return cmd_msg;
}





void portSUPPRESS_TICKS_AND_SLEEP( TickType_t xExpectedIdleTime )
{
	//     //Disable interrupts so nothing changes while we prepare to sleep /
	//    __disable_irq();
	//
	//    // Confirm it is still ok to sleep /
	//    if( eTaskConfirmSleepModeStatus() == eAbortSleep )
	//    {
	//        __enable_irq();
	//        return;
	//    }
	//
	//    // Stop SysTick /
	//    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	//
	//    // Re-enable interrupts before sleeping /
	//    __enable_irq();
	//
	//    // Sleep until interrupt /
	//    __WFI();
	//
	//    // Restart SysTick
	//    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}


/*
 * Clock configuration for the Nucleo STM32F072RB board
 * HSE input Bypass Mode            -> 8MHz
 * SYSCLK, AHB, APB1                -> 48MHz
 * PA8 as MCO with /16 prescaler    -> 3MHz
 */

static void SystemClock_Config()
{
	uint32_t    HSE_Status;
	uint32_t    PLL_Status;
	uint32_t    SW_Status;
	uint32_t    timeout = 0;
	timeout = 1000000;

	// Start HSE in Bypass Mode
	RCC->CR |= RCC_CR_HSEBYP;
	RCC->CR |= RCC_CR_HSEON;

	// Wait until HSE is ready
	do
	{
		HSE_Status = RCC->CR & RCC_CR_HSERDY_Msk;
		timeout--;
	} while ((HSE_Status == 0) && (timeout > 0));

	// Select HSE as PLL input source
	RCC->CFGR &= ~RCC_CFGR_PLLSRC_Msk;
	RCC->CFGR |= (0x02 <<RCC_CFGR_PLLSRC_Pos);

	// Set PLL PREDIV to /1
	RCC->CFGR2 = 0x00000000;

	// Set PLL MUL to x6
	RCC->CFGR &= ~RCC_CFGR_PLLMUL_Msk;
	RCC->CFGR |= (0x04 <<RCC_CFGR_PLLMUL_Pos);

	// Enable the main PLL
	RCC-> CR |= RCC_CR_PLLON;
	// Wait until PLL is ready
	do
	{
		PLL_Status = RCC->CR & RCC_CR_PLLRDY_Msk;
		timeout--;
	} while ((PLL_Status == 0) && (timeout > 0));

	// Set AHB prescaler to /1
	RCC->CFGR &= ~RCC_CFGR_HPRE_Msk;
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

	//Set APB1 prescaler to /1
	RCC->CFGR &= ~RCC_CFGR_PPRE_Msk;
	RCC->CFGR |= RCC_CFGR_PPRE_DIV1;

	// Enable FLASH Prefetch Buffer and set Flash Latency
	FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

	// Select the main PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	// Wait until PLL becomes main switch input
	do
	{
		SW_Status = (RCC->CFGR & RCC_CFGR_SWS_Msk);
		timeout--;
	} while ((SW_Status != RCC_CFGR_SWS_PLL) && (timeout > 0));

	// Update SystemCoreClock global variable
	SystemCoreClockUpdate();
}

