#ifndef INC_LED_H_
#define INC_LED_H_
#include "gpio.h"

#define LED_INIT_STATE_OFF	(0)
#define LED_INIT_STATE_ON	(1)
#define LED_TOGGLE_PERIOD	(0)

#define LED_ERR		(-1)
#define LED_OK		(0)

/**
 * @brief Tipo di dato per lo stato del LED, coerente con le definizioni HAL dei Pin GPIO.
 */
typedef GPIO_PinState led_state_t;

/**
 * @brief Struttura che mappa la configurazione e lo stato operativo di un LED hardware.
 */
struct led_s
{
	GPIO_TypeDef* GPIOx;       /**< Porta GPIO hardware associata al LED (es. GPIOA) */
	uint16_t GPIO_Pin;         /**< Numero del pin GPIO associato al LED (es. GPIO_PIN_5) */
	led_state_t state;         /**< Stato logico e di scrittura corrente del LED */
	uint32_t toggle_period;    /**< Periodo di lampeggio (toggling) desiderato in millisecondi */
	uint32_t last_tick;        /**< Ultimo timestamp HAL_GetTick() in cui è avvenuta la commutazione di stato */
};

typedef struct led_s led_t;

/**
 * @brief Inizializza la struttura del LED e ne scrive immediatamente lo stato iniziale sul pin hardware.
 * @note La memoria della struttura led_t deve essere allocata esternamente prima della chiamata.
 * * @param led Puntatore alla struttura led_t da inizializzare.
 * @param GPIOx Porta GPIO hardware a cui è connesso il LED.
 * @param GPIO_Pin Pin GPIO hardware a cui è connesso il LED.
 * @param init_state Stato elettrico iniziale del LED (LED_INIT_STATE_OFF o LED_INIT_STATE_ON).
 * @return int8_t LED_OK se l'inizializzazione e la prima scrittura avvengono con successo, LED_ERR se il puntatore è nullo.
 */
int8_t led_init(led_t* led, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, led_state_t init_state);

/**
 * @brief Imposta direttamente uno stato logico specifico per il LED, aggiornando sia la struttura che l'hardware.
 * * @param led Puntatore all'istanza del LED.
 * @param status Stato da scrivere sul pin (GPIO_PIN_RESET o GPIO_PIN_SET).
 * @return int8_t LED_OK in caso di successo, LED_ERR se il puntatore è nullo.
 */
int8_t led_write(led_t* led, led_state_t status);

/**
 * @brief Accende il LED.
 * * @param led Puntatore all'istanza del LED.
 * @return int8_t LED_OK in caso di successo, LED_ERR in caso di puntatore non valido.
 */
int8_t led_on(led_t* led);

/**
 * @brief Spegne il LED.
 * * @param led Puntatore all'istanza del LED.
 * @return int8_t LED_OK in caso di successo, LED_ERR in caso di puntatore non valido.
 */
int8_t led_off(led_t* led);

/**
 * @brief Configura il periodo di commutazione temporale (lampeggio) per la funzione led_toggle.
 * * @param led Puntatore all'istanza del LED.
 * @param toggle_period Periodo desiderato espresso in millisecondi.
 * @return int8_t LED_OK in caso di successo, LED_ERR se il puntatore è nullo.
 */
int8_t led_set_toggle_period(led_t* led, uint32_t toggle_period);

/**
 * @brief Recupera il periodo di commutazione (lampeggio) attualmente memorizzato nella struttura.
 * * @param led Puntatore all'istanza del LED.
 * @param toggle_period Puntatore alla variabile uint32_t destinata a ricevere il periodo in millisecondi.
 * @return int8_t LED_OK in caso di successo, LED_ERR se uno dei puntatori è nullo.
 */
int8_t led_get_toggle_period(led_t* led, uint32_t* toggle_period);

/**
 * @brief Inverte (commuta) lo stato del LED se è trascorso il periodo di tempo configurato.
 * @details Verifica la differenza di tempo non bloccante rispetto a `last_tick` basandosi su HAL_GetTick().
 * Se il tempo è maturo, esegue l'inversione logica del pin hardware e aggiorna il timestamp interno.
 * * @param led Puntatore all'istanza del LED.
 * @return int8_t LED_OK in caso di operazione completata (sia in caso di toggle avvenuto che non), LED_ERR se il puntatore è nullo.
 */
int8_t led_toggle(led_t* led);

/**
 * @brief Restituisce l'ultimo stato logico memorizzato all'interno della struttura dati del LED.
 * * @param led Puntatore costante all'istanza del LED.
 * @param state Puntatore alla variabile che riceverà lo stato salvato.
 * @return int8_t LED_OK in caso di successo, LED_ERR se i parametri sono nulli.
 */
int8_t led_get_status(const led_t* led, led_state_t* state);

/**
 * @brief Restituisce il puntatore alla porta GPIO hardware (GPIO_TypeDef) utilizzata dal LED.
 * * @param led Puntatore costante all'istanza del LED.
 * @param port Puntatore a un puntatore GPIO_TypeDef che riceverà l'indirizzo della porta.
 * @return int8_t LED_OK in caso di successo, LED_ERR se i parametri sono nulli.
 */
int8_t led_get_port(const led_t* led, GPIO_TypeDef** port);

/**
 * @brief Restituisce il numero del pin GPIO hardware utilizzato dal LED.
 * * @param led Puntatore costante all'istanza del LED.
 * @param number Puntatore alla variabile uint16_t che riceverà il numero del pin.
 * @return int8_t LED_OK in caso di successo, LED_ERR se i parametri sono nulli.
 */
int8_t led_get_pin_number(const led_t* led, uint16_t* number);

#endif
