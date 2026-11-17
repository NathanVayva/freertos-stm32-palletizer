#ifndef INC_MAIN_H_
#define INC_MAIN_H_

#define CMD_MSG_SIZE	(60)
#define SS_MSG_SIZE		(14)
// Device header
#include "stm32f0xx.h"

// BSP functions
#include "bsp.h"

// FreeRTOS headers
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"
#include "stream_buffer.h"

// Global Functions
int my_printf	(const char *format, ...);
int my_sprintf	(char *out, const char *format, ...);

// Global defines
//Define actuators
#define Distribution_Cartons 	  		1
#define Tapis_Distribution_Cartons 	  	1 << 1
#define Blocage_Entree_Palettiseur  	1 << 2
#define Porte 	  						1 << 3
#define Poussoir 	  					1 << 4
#define Clamp 	  						1 << 5
#define Monter_Ascenseur 	  			1 << 6

#define Descendre_Ascenseur 	  		1 << 8
#define Ascenseur_To_Limit 	  			1 << 9
#define Distribution_Palette 	  		1 << 10
#define Charger_Palette 	  			1 << 11
#define Tapis_Carton_vers_Palettiseur	1 << 12
#define Tourner_Carton 	  				1 << 13
#define Decharger_Palettiseur 	  		1 << 14

#define Charger_Palettiseur 	  		1 << 16
#define Décharger_Palette 	  			1 << 17
#define Tapis_Palette_Vers_Ascenseur 	1 << 18
#define Tapis_Distribution_Palette 	  	1 << 19
#define Tapis_Fin 	  					1 << 20
#define Remover 	  					1 << 21


//Define sensors
#define Carton_Distribue 	  			1
#define Carton_Envoye 	  				1 << 1
#define Entree_Palettiseur			  	1 << 2
#define Porte_Ouverte 	  				1 << 3
#define Limite_Poussoir 	  			1 << 4
#define Clamped 	  					1 << 5
#define Ascenseur_Etage_RDC 	  		1 << 6

#define Ascenseur_Etage_1 	  			1 << 8
#define Ascenseur_Etage_2 	  			1 << 9
#define Sortie_Palette 	  				1 << 10
#define Limite_Porte 	  				1 << 11
#define Ascenseur_en_mouvement			1 << 12
#define Entree_Palette 	  				1 << 13
#define Butee_Carton 	  				1 << 14


#endif /* INC_MAIN_H_ */
