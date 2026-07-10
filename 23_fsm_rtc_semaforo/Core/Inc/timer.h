/*
 * timer.h
 *
 *  Created on: Mar 26, 2023
 *      Author: vincarlet
 */

#ifndef INC_TIMER_H_
#define INC_TIMER_H_
#include "stm32g4xx_hal.h"

#define TIMER_OK	(0)
#define TIMER_ERR	(-1)

/**
 * @brief Struttura che mappa la configurazione e lo stato operativo di un Timer hardware.
 * @note Se i campi di flag come `running` ed `elapsed` vengono modificati all'interno delle
 * Callback di Interrupt (ISR), dovrebbero essere trattati con le dovute cautele di atomicità.
 */
struct timer_s
{
	TIM_HandleTypeDef* htim;    /**< Puntatore all'handler del peripheral hardware di ST (es. &htim2) */
	uint8_t running;            /**< Flag booleano che indica se il timer è attualmente attivo (1) o fermo (0) */
	uint16_t elapsed;           /**< Flag booleano che indica se l'evento di timeout/periodo è scaduto */
	uint32_t in_frequency;      /**< Frequenza di clock in ingresso al timer espressa in Hz (usata per il calcolo dell'ARR) */
	uint8_t timer_elapsed_first;	/**< Flag di controllo software utilizzato per ignorare il primo interrupt fittizio del timer */
};

typedef struct timer_s timer_t;

/**
 * @brief Inizializza la struttura del timer con l'handler hardware e la frequenza di clock nominale.
 * @note Questa funzione NON alloca la memoria per la struttura; l'allocazione deve essere gestita esternamente.
 * * @param[out] timer     Puntatore alla struttura timer_t da inizializzare.
 * @param[in]  handler   Puntatore all'handler software STM32 HAL del Timer hardware.
 * @param[in]  frequency Frequenza del clock di calcolo espressa in Hz.
 * @return int8_t TIMER_OK in caso di successo, TIMER_ERR se uno dei puntatori in ingresso è nullo.
 */
int8_t timer_init(timer_t* timer, TIM_HandleTypeDef* handler, uint32_t frequency);

/**
 * @brief Avvia il timer abilitando il relativo interrupt hardware sull'evento di update.
 * * @param[in,out] timer Puntatore alla struttura dell'istanza del timer.
 * @return int8_t TIMER_OK in caso di successo, TIMER_ERR se il puntatore è nullo.
 */
int8_t timer_start(timer_t* timer);

/**
 * @brief Sospende il conteggio del timer disabilitando l'interrupt hardware associato.
 * @note Mantiene inalterato il valore corrente del contatore hardware (CNT).
 * * @param[in,out] timer Puntatore alla struttura dell'istanza del timer.
 * @return int8_t TIMER_OK in caso di successo, TIMER_ERR se il puntatore è nullo.
 */
int8_t timer_stop(timer_t* timer);

/**
 * @brief Sospende il timer, disabilita l'interrupt e azzera il registro del contatore hardware.
 * * @param[in,out] timer Puntatore alla struttura dell'istanza del timer.
 * @return int8_t TIMER_OK in caso di successo, TIMER_ERR se il puntatore è nullo.
 */
int8_t timer_reset(timer_t* timer);

/**
 * @brief Verifica se il timer è attualmente in uno stato attivo e in esecuzione.
 * * @param[in] timer Puntatore alla struttura dell'istanza del timer.
 * @return int8_t Restituisce 1 se il timer è in esecuzione, 0 se è fermo o se il puntatore è nullo.
 */
int8_t timer_is_running(timer_t* timer);

/**
 * @brief Controlla se il periodo impostato è scaduto (evento di timeout generato).
 * * @param[in] timer Puntatore alla struttura dell'istanza del timer.
 * @return int8_t Restituisce il valore del flag di timeout `elapsed` (1 se scaduto, 0 altrimenti o se il puntatore è nullo).
 */
int8_t timer_is_elapsed(timer_t* timer);

/**
 * @brief Verifica se l'istanza del timer corrisponde a un determinato handler hardware HAL.
 * * @param[in] timer   Puntatore alla struttura dell'istanza del timer.
 * @param[in] handler Puntatore all'handler dell'interfaccia HAL da confrontare.
 * @return int8_t TIMER_OK se l'istanza hardware coincide, TIMER_ERR se differiscono o se i parametri sono nulli.
 */
int8_t timer_is_my_handler(timer_t* timer, TIM_HandleTypeDef* handler);

/**
 * @brief Configura dinamicamente il periodo del timer calcolando e impostando il registro Auto-Reload (ARR).
 * @details Legge il Prescaler hardware corrente e calcola il valore ARR matematicamente
 * in base al tempo desiderato e alla frequenza memorizzata nell'istanza.
 * * @param[in,out] timer  Puntatore alla struttura dell'istanza del timer.
 * @param[in]     period Durata del periodo desiderato espressa in secondi.
 * @return int8_t TIMER_OK in caso di successo, TIMER_ERR se il puntatore è nullo.
 */
int8_t timer_set_period(timer_t* timer, uint16_t period);

/**
 * @brief Gestore dell'evento di fine periodo da invocare all'interno delle ISR (Interrupt Service Routine).
 * @details Verifica se l'handler che ha scatenato la callback appartiene a questa istanza;
 * in caso positivo ferma lo stato di running e imposta a 1 il flag `elapsed`.
 * * @param[in,out] timer   Puntatore alla struttura dell'istanza del timer.
 * @param[in]     handler Puntatore all'handler hardware che ha generato l'interrupt di periodo.
 * @return int8_t TIMER_OK se l'interrupt apparteneva a questo timer (flag aggiornati), TIMER_ERR in caso contrario o se i parametri sono nulli.
 */
int8_t timer_elapsed_handler(timer_t* timer, TIM_HandleTypeDef* handler);

#endif /* INC_TIMER_H_ */
